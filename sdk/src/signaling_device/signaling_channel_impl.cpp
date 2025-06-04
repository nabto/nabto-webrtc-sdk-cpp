#include "signaling_channel_impl.hpp"

#include "logging.hpp"
#include "signaling_device_impl.hpp"

#include "nabto/signaling/signaling.hpp"

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
    auto type = msg["type"].get<std::string>();
    if (type == "DATA") {
      NABTO_SIGNALING_LOGD << "Handling DATA";
      sendAck(msg);
      if (signalingMessageHandler_) {
        const auto& str = msg["data"];
        signalingMessageHandler_(str);
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
  if (signalingEventHandler_) {
    signalingEventHandler_(SignalingChannelState::CLOSED);
  }
  signalingMessageHandler_ = nullptr;
  signalingEventHandler_ = nullptr;
  signalingErrorHandler_ = nullptr;
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
                              {"seq", msg["seq"].get<uint32_t>()}};
  signaler_->websocketSendMessage(channelId_, ack);
}

void SignalingChannelImpl::handleAck(const nlohmann::json& msg) {
  if (unackedMessages_.empty()) {
    NABTO_SIGNALING_LOGE << "Got an ack but we have no unacked messages";
    return;
  }
  auto firstItem = unackedMessages_[0];
  try {
    if (firstItem["seq"].get<uint32_t>() != msg["seq"].get<uint32_t>()) {
      NABTO_SIGNALING_LOGE << "Got an ack for seq "
                           << msg["seq"].get<uint32_t>()
                           << " but the first item in unacked was seq: "
                           << firstItem["seq"].get<uint32_t>();
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
  if (signalingEventHandler_) {
    signalingEventHandler_(SignalingChannelState::ONLINE);
  }
}

void SignalingChannelImpl::peerOffline() {
  NABTO_SIGNALING_LOGI << "Peer: " << channelId_ << " went offline";
  if (signalingEventHandler_) {
    signalingEventHandler_(SignalingChannelState::OFFLINE);
  }
}

void SignalingChannelImpl::handleError(const SignalingError& error) {
  NABTO_SIGNALING_LOGI << "Got error: (" << error.errorCode() << ") "
                       << error.errorMessage() << " from Peer: " << channelId_;
  if (signalingErrorHandler_) {
    signalingErrorHandler_(error);
  }
}

void SignalingChannelImpl::close() {
  signaler_->channelClosed(channelId_);
  // signaler_ = nullptr;
  signalingMessageHandler_ = nullptr;
  signalingEventHandler_ = nullptr;
  signalingErrorHandler_ = nullptr;
}

bool SignalingChannelImpl::isInitialMessage(const nlohmann::json& msg) {
  try {
    auto type = msg["type"].get<std::string>();
    auto seq = msg["seq"].get<int>();
    return type == "DATA" && seq == 0;
  } catch (std::exception& e) {
    return false;
  }
}

std::string SignalingChannelImpl::getChannelId() { return channelId_; }

}  // namespace signaling
}  // namespace nabto
