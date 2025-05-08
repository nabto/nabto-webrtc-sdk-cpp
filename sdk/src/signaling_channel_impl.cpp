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

SignalingChannelImplPtr SignalingChannelImpl::create(SignalingDeviceImplPtr signaler, const std::string& channelId, bool isDevice, bool isAuthorized) {
    return std::make_shared<SignalingChannelImpl>(std::move(signaler), channelId, isDevice, isAuthorized);
}

SignalingChannelImpl::SignalingChannelImpl(SignalingDeviceImplPtr signaler, std::string channelId, bool isDevice, bool isAuthorized) : signaler_(std::move(signaler)), channelId_(std::move(channelId)), isDevice_(isDevice), isAuthorized_(isAuthorized) {
}

void SignalingChannelImpl::handleMessage(const std::string& msg) {
    try {
        nlohmann::json parsedMsg = nlohmann::json::parse(msg);
        auto type = parsedMsg["type"].get<std::string>();
        if (type == "MESSAGE") {
            NABTO_SIGNALING_LOGD << "Handling MESSAGE";
            sendAck(parsedMsg);
            if (signalingMessageHandler_) {
                auto str = parsedMsg["message"].get<std::string>();
                signalingMessageHandler_(str);
            }
        } else if (type == "ACK") {
            NABTO_SIGNALING_LOGD << "Handling ACK";
            handleAck(parsedMsg);
        } else {
            NABTO_SIGNALING_LOGE << "Got unknown signaling message type: " << type;
        }
    } catch (std::exception& ex) {
        NABTO_SIGNALING_LOGE << "Failed to parse Signaling message: " << msg << " with error: " << ex.what();
    }
}

void SignalingChannelImpl::wsReconnected() {
    for (auto const& message : unackedMessages_) {
        signaler_->websocketSendMessage(channelId_, message.dump());
    }
    if (signalingEventHandler_) {
        signalingEventHandler_(SIGNALING_RECONNECT);
    }
}

void SignalingChannelImpl::wsClosed() {
    if (signalingEventHandler_) {
        signalingEventHandler_(SIGNALING_CLOSED);
    }
    signalingMessageHandler_ = nullptr;
    signalingEventHandler_ = nullptr;
    signalingErrorHandler_ = nullptr;
}

void SignalingChannelImpl::sendMessage(const std::string& message) {
    const nlohmann::json root = {
        {"type", "MESSAGE"},
        {"seq", sendSeq_},
        {"message", message}};
    sendSeq_++;
    unackedMessages_.push_back(root);
    signaler_->websocketSendMessage(channelId_, root.dump());
}

void SignalingChannelImpl::sendError(const SignalingError& error) {
    signaler_->websocketSendError(channelId_, error);
}

void SignalingChannelImpl::sendAck(nlohmann::json& msg) {
    const nlohmann::json ack = {{"type", "ACK"}, {"seq", msg["seq"].get<uint32_t>()}};
    signaler_->websocketSendMessage(channelId_, ack.dump());
}

void SignalingChannelImpl::handleAck(nlohmann::json& msg) {
    if (unackedMessages_.empty()) {
        NABTO_SIGNALING_LOGE << "Got an ack but we have no unacked messages";
        return;
    }
    auto firstItem = unackedMessages_[0];
    try {
        if (firstItem["seq"].get<uint32_t>() != msg["seq"].get<uint32_t>()) {
            NABTO_SIGNALING_LOGE << "Got an ack for seq " << msg["seq"].get<uint32_t>() << " but the first item in unacked was seq: " << firstItem["seq"].get<uint32_t>();
            return;
        }
        unackedMessages_.erase(unackedMessages_.begin());
    } catch (std::exception& ex) {
        NABTO_SIGNALING_LOGE << "Failed to handle ACK: " << msg.dump() << " with: " << ex.what();
    }
}

void SignalingChannelImpl::peerConnected() {
    for (auto const& message : unackedMessages_) {
        signaler_->websocketSendMessage(channelId_, message.dump());
    }
    if (signalingEventHandler_) {
        signalingEventHandler_(SignalingChannelEventType::CLIENT_CONNECTED);
    }
}

void SignalingChannelImpl::peerOffline() {
    NABTO_SIGNALING_LOGI << "Peer: " << channelId_ << " went offline";
    if (signalingEventHandler_) {
        signalingEventHandler_(SignalingChannelEventType::CLIENT_OFFLINE);
    }
}

void SignalingChannelImpl::handleError(const SignalingError& error) {
    NABTO_SIGNALING_LOGI << "Got error: (" << error.errorCode() << ") " << error.errorMessage() << " from Peer: " << channelId_;
    if (signalingErrorHandler_) {
        signalingErrorHandler_(error);
    }
}

void SignalingChannelImpl::checkAlive() {
    signaler_->checkAlive();
}

void SignalingChannelImpl::requestIceServers(IceServersResponse callback) {
    signaler_->requestIceServers(callback);
}

void SignalingChannelImpl::close() {
    signaler_->channelClosed(channelId_);
    // signaler_ = nullptr;
    signalingMessageHandler_ = nullptr;
    signalingEventHandler_ = nullptr;
    signalingErrorHandler_ = nullptr;
}

bool SignalingChannelImpl::isInitialMessage(const std::string& msg) {
    try {
        nlohmann::json json = nlohmann::json::parse(msg);
        auto type = json["type"].get<std::string>();
        auto seq = json["seq"].get<int>();
        return type == "MESSAGE" && seq == 0;
    } catch (std::exception& e) {
        return false;
    }
}

}  // namespace signaling
}  // namespace nabto
