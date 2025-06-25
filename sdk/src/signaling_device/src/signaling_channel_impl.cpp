#include "signaling_channel_impl.hpp"

#include "logging.hpp"
#include "signaling_device_impl.hpp"

#include <nabto/webrtc/device.hpp>

#include <nlohmann/json_fwd.hpp>

#include <cstdint>
#include <exception>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

namespace nabto {
namespace signaling {

SignalingChannelImplPtr SignalingChannelImpl::create(
    SignalingDeviceImplPtr signaler, const std::string& channelId) {
  return std::make_shared<SignalingChannelImpl>(std::move(signaler), channelId);
}

SignalingChannelImpl::SignalingChannelImpl(SignalingDeviceImplPtr signaler,
                                           std::string channelId)
    : signaler_(std::move(signaler)), channelId_(std::move(channelId)) {}

void SignalingChannelImpl::handleMessage(const nlohmann::json& msg) {
  try {
    auto type = msg.at("type").get<std::string>();
    if (type == "DATA") {
      NABTO_SIGNALING_LOGD << "Handling DATA";
      sendAck(msg);
      const auto& str = msg.at("data");
      std::map<MessageListenerId, SignalingMessageHandler> messageHandlers;
      {
        const std::lock_guard<std::mutex> lock(mutex_);
        messageHandlers = messageHandlers_;
      }
      for (const auto& [id, handler] : messageHandlers) {
        handler(str);
      }
    } else if (type == "ACK") {
      NABTO_SIGNALING_LOGD << "Handling ACK";
      handleAck(msg);
    } else {
      NABTO_SIGNALING_LOGE << "Got unknown signaling message type: " << type;
    }
  } catch (std::exception& ex) {
    NABTO_SIGNALING_LOGE << "Failed to parse Signaling message: " << msg.dump()
                         << " with error: " << ex.what();
  }
}

void SignalingChannelImpl::wsClosed() {
  changeState(SignalingChannelState::CLOSED);
  const std::lock_guard<std::mutex> lock(mutex_);
  messageHandlers_.clear();
  stateHandlers_.clear();
  errorHandlers_.clear();
}

void SignalingChannelImpl::sendMessage(const nlohmann::json& message) {
  const std::lock_guard<std::mutex> lock(mutex_);
  if (stateIsEnded()) {
    NABTO_SIGNALING_LOGE << "sendMessage called from invalid state";
    return;
  }
  const nlohmann::json root = {
      {"type", "DATA"}, {"seq", sendSeq_}, {"data", message}};
  sendSeq_++;
  unackedMessages_.push_back(root);
  signaler_->websocketSendMessage(channelId_, root);
}

void SignalingChannelImpl::sendError(const SignalingError& error) {
  {
    const std::lock_guard<std::mutex> lock(mutex_);
    if (stateIsEnded()) {
      NABTO_SIGNALING_LOGE << "sendError called from invalid state";
      return;
    }
    signaler_->websocketSendError(channelId_, error);
  }
  changeState(SignalingChannelState::FAILED);
}

void SignalingChannelImpl::sendAck(const nlohmann::json& msg) {
  const nlohmann::json ack = {{"type", "ACK"},
                              {"seq", msg.at("seq").get<uint32_t>()}};
  const std::lock_guard<std::mutex> lock(mutex_);
  signaler_->websocketSendMessage(channelId_, ack);
}

void SignalingChannelImpl::handleAck(const nlohmann::json& msg) {
  const std::lock_guard<std::mutex> lock(mutex_);
  if (unackedMessages_.empty()) {
    NABTO_SIGNALING_LOGE << "Got an ack but we have no unacked messages";
    return;
  }
  auto firstItem = unackedMessages_[0];
  try {
    if (firstItem.at("seq").get<uint32_t>() != msg.at("seq").get<uint32_t>()) {
      NABTO_SIGNALING_LOGE << "Got an ack for seq "
                           << msg.at("seq").get<uint32_t>()
                           << " but the first item in unacked was seq: "
                           << firstItem.at("seq").get<uint32_t>();
      return;
    }
    unackedMessages_.erase(unackedMessages_.begin());
  } catch (std::exception& ex) {
    NABTO_SIGNALING_LOGE << "Failed to handle ACK: " << msg.dump()
                         << " with: " << ex.what();
  }
}

void SignalingChannelImpl::peerConnected() {
  {
    const std::lock_guard<std::mutex> lock(mutex_);

    for (auto const& message : unackedMessages_) {
      signaler_->websocketSendMessage(channelId_, message);
    }
  }
  changeState(SignalingChannelState::ONLINE);
}

void SignalingChannelImpl::peerOffline() {
  {
    const std::lock_guard<std::mutex> lock(mutex_);

    NABTO_SIGNALING_LOGI << "Peer: " << channelId_ << " went offline";
  }
  changeState(SignalingChannelState::OFFLINE);
}

void SignalingChannelImpl::handleError(const SignalingError& error) {
  std::map<ChannelErrorListenerId, SignalingErrorHandler> errorHandlers;
  {
    const std::lock_guard<std::mutex> lock(mutex_);
    NABTO_SIGNALING_LOGI << "Got error: (" << error.errorCode() << ") "
                         << error.errorMessage()
                         << " from Peer: " << channelId_;
    if (stateIsEnded()) {
      NABTO_SIGNALING_LOGI
          << "Got error while in error state. Not reinvoking handlers";
    } else {
      errorHandlers = errorHandlers_;
    }
  }
  for (const auto& [id, handler] : errorHandlers) {
    handler(error);
  }
}

void SignalingChannelImpl::close() {
  {
    const std::lock_guard<std::mutex> lock(mutex_);
    if (stateIsEnded()) {
      return;
    }
  }
  changeState(SignalingChannelState::CLOSED);
  {
    const std::lock_guard<std::mutex> lock(mutex_);
    signaler_->channelClosed(channelId_);
    messageHandlers_.clear();
    stateHandlers_.clear();
    errorHandlers_.clear();
  }
}

bool SignalingChannelImpl::isInitialMessage(const nlohmann::json& msg) {
  try {
    auto type = msg.at("type").get<std::string>();
    auto seq = msg.at("seq").get<int>();
    return type == "DATA" && seq == 0;
  } catch (std::exception& e) {
    return false;
  }
}

std::string SignalingChannelImpl::getChannelId() {
  const std::lock_guard<std::mutex> lock(mutex_);
  return channelId_;
}

void SignalingChannelImpl::changeState(SignalingChannelState state) {
  std::map<ChannelStateListenerId, SignalingChannelStateHandler> stateHandlers;
  {
    const std::lock_guard<std::mutex> lock(mutex_);
    if (state != state_) {
      stateHandlers = stateHandlers_;
      state_ = state;
    }
  }
  for (const auto& [id, handler] : stateHandlers) {
    handler(state);
  }
}

}  // namespace signaling
}  // namespace nabto
