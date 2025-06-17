#include "signaling_channel_impl.hpp"

#include "logging.hpp"
#include "signaling_device_impl.hpp"

#include <nabto/webrtc/device.hpp>

#include <nlohmann/json_fwd.hpp>

#include <cstdint>
#include <exception>
#include <memory>
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
      for (const auto& [id, handler] : messageHandlers_) {
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
  for (const auto& [id, handler] : stateHandlers_) {
    handler(SignalingChannelState::CLOSED);
  }
  messageHandlers_.clear();
  stateHandlers_.clear();
  errorHandlers_.clear();
}

void SignalingChannelImpl::sendMessage(const nlohmann::json& message) {
  const nlohmann::json root = {
      {"type", "DATA"}, {"seq", sendSeq_}, {"data", message}};
  sendSeq_++;
  unackedMessages_.push_back(root);
  signaler_->websocketSendMessage(channelId_, root);
}

void SignalingChannelImpl::sendError(const SignalingError& error) {
  signaler_->websocketSendError(channelId_, error);
}

void SignalingChannelImpl::sendAck(const nlohmann::json& msg) {
  const nlohmann::json ack = {{"type", "ACK"},
                              {"seq", msg.at("seq").get<uint32_t>()}};
  signaler_->websocketSendMessage(channelId_, ack);
}

void SignalingChannelImpl::handleAck(const nlohmann::json& msg) {
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
  for (auto const& message : unackedMessages_) {
    signaler_->websocketSendMessage(channelId_, message);
  }
  for (const auto& [id, handler] : stateHandlers_) {
    handler(SignalingChannelState::ONLINE);
  }
}

void SignalingChannelImpl::peerOffline() {
  NABTO_SIGNALING_LOGI << "Peer: " << channelId_ << " went offline";
  for (const auto& [id, handler] : stateHandlers_) {
    handler(SignalingChannelState::OFFLINE);
  }
}

void SignalingChannelImpl::handleError(const SignalingError& error) {
  NABTO_SIGNALING_LOGI << "Got error: (" << error.errorCode() << ") "
                       << error.errorMessage() << " from Peer: " << channelId_;
  for (const auto& [id, handler] : errorHandlers_) {
    handler(error);
  }
}

void SignalingChannelImpl::close() {
  signaler_->channelClosed(channelId_);
  messageHandlers_.clear();
  stateHandlers_.clear();
  errorHandlers_.clear();
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

std::string SignalingChannelImpl::getChannelId() { return channelId_; }

}  // namespace signaling
}  // namespace nabto
