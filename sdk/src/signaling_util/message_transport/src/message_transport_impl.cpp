#include "message_transport_impl.hpp"

#include "message_signer.hpp"
#include "none_message_signer.hpp"
#include "shared_secret_message_signer.hpp"

#include <nabto/webrtc/device.hpp>
#include <nabto/webrtc/util/logging.hpp>
#include <nabto/webrtc/util/message_transport.hpp>

#include <nlohmann/json.hpp>

#include <exception>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace nabto {
namespace webrtc {
namespace util {

MessageTransportPtr MessageTransportImpl::createNone(
    nabto::webrtc::SignalingDevicePtr device,
    nabto::webrtc::SignalingChannelPtr channel) {
  auto ptr = std::make_shared<MessageTransportImpl>(std::move(device),
                                                    std::move(channel));
  if (ptr) {
    ptr->init();
  }
  return ptr;
}

MessageTransportPtr MessageTransportImpl::createSharedSecret(
    nabto::webrtc::SignalingDevicePtr device,
    nabto::webrtc::SignalingChannelPtr channel,
    MessageTransportSharedSecretHandler sharedSecretHandler) {
  auto ptr = std::make_shared<MessageTransportImpl>(
      std::move(device), std::move(channel), std::move(sharedSecretHandler));
  if (ptr) {
    ptr->init();
  }
  return ptr;
}

MessageTransportImpl::MessageTransportImpl(
    nabto::webrtc::SignalingDevicePtr device,
    nabto::webrtc::SignalingChannelPtr channel)
    : device_(std::move(device)),
      channel_(std::move(channel)),
      mode_(SigningMode::NONE) {}

MessageTransportImpl::MessageTransportImpl(
    nabto::webrtc::SignalingDevicePtr device,
    nabto::webrtc::SignalingChannelPtr channel,
    MessageTransportSharedSecretHandler sharedSecretHandler)
    : device_(std::move(device)),
      channel_(std::move(channel)),
      sharedSecretHandler_(std::move(sharedSecretHandler)),
      mode_(SigningMode::SHARED_SECRET) {}

void MessageTransportImpl::init() {
  auto self = shared_from_this();
  channel_->addMessageListener(
      // for some reason clang tidy complains that nlohmann is not directly
      // included here. It does not fail 3 lines later but it does fail here.
      // NOLINTNEXTLINE(misc-include-cleaner)
      [self](const nlohmann::json& msg) { self->handleMessage(msg); });
}

void MessageTransportImpl::handleMessage(const nlohmann::json& msgIn) {
  try {
    if (!signer_) {
      setupSigner(msgIn);
    }
  } catch (std::exception& ex) {
    NPLOGE << "Failed to setup signer: " << ex.what();
    auto err = nabto::webrtc::SignalingError(
        nabto::webrtc::SignalingErrorCode::INTERNAL_ERROR, "Internal error");
    handleError(err);
    return;
  }
  try {
    NPLOGI << "Webrtc got signaling message IN: " << msgIn.dump();
    auto msg = signer_->verifyMessage(msgIn);

    NPLOGI << "Webrtc got signaling message: " << msg.dump();
    auto type = msg.at("type").get<std::string>();
    if (type == "SETUP_REQUEST") {
      requestIceServers();
    } else if (type == "DESCRIPTION" || type == "CANDIDATE") {
      if (msgHandlers_.empty()) {
        NPLOGE << "Received signaling message without a registered message "
                  "handler";
      } else {
        std::map<TransportMessageListenerId, MessageTransportMessageHandler>
            msgHandlers;
        {
          const std::lock_guard<std::mutex> lock(handlerLock_);
          msgHandlers = msgHandlers_;
        }
        auto sigMsg = WebrtcSignalingMessage::fromJson(msg);
        for (const auto& [id, handler] : msgHandlers) {
          handler(sigMsg);
        }
      }
    }
  } catch (nabto::webrtc::util::VerificationError& ex) {
    NPLOGE << "Could not verify the incoming signaling message: "
           << msgIn.dump() << " with: " << ex.what();
    auto err = nabto::webrtc::SignalingError(
        nabto::webrtc::SignalingErrorCode::VERIFICATION_ERROR,
        "Could not verify the incoming signaling message");
    handleError(err);
  } catch (nabto::webrtc::util::DecodeError& ex) {
    NPLOGE << "Could not decode the incoming signaling message: "
           << msgIn.dump() << " with: " << ex.what();
    auto err = nabto::webrtc::SignalingError(
        nabto::webrtc::SignalingErrorCode::DECODE_ERROR,
        "Could not decode the incoming signaling message");
    handleError(err);
  } catch (nlohmann::json::exception& ex) {
    NPLOGE << "Failed to parse json: " << ex.what()
           << " for msg: " << msgIn.dump();
    auto err = nabto::webrtc::SignalingError(
        nabto::webrtc::SignalingErrorCode::DECODE_ERROR,
        "Could not decode the incoming signaling message");
    handleError(err);

  } catch (std::exception& ex) {
    NPLOGE << "Failed to handle message: " << msgIn.dump()
           << " with: " << ex.what();
    auto err = nabto::webrtc::SignalingError(
        nabto::webrtc::SignalingErrorCode::INTERNAL_ERROR, "Internal error");
    handleError(err);
  }
};

SetupDoneListenerId MessageTransportImpl::addSetupDoneListener(
    SetupDoneHandler handler) {
  const std::lock_guard<std::mutex> lock(handlerLock_);
  const SetupDoneListenerId id = currSetupListId_;
  currSetupListId_++;
  setupHandlers_.insert({id, handler});
  return id;
}

TransportMessageListenerId MessageTransportImpl::addMessageListener(
    MessageTransportMessageHandler handler) {
  const std::lock_guard<std::mutex> lock(handlerLock_);
  const TransportMessageListenerId id = currMsgListId_;
  currMsgListId_++;
  msgHandlers_.insert({id, handler});
  return id;
}

TransportErrorListenerId MessageTransportImpl::addErrorListener(
    nabto::webrtc::SignalingErrorHandler handler) {
  const std::lock_guard<std::mutex> lock(handlerLock_);
  const TransportErrorListenerId id = currErrListId_;
  currErrListId_++;
  errHandlers_.insert({id, handler});
  return id;
}

void MessageTransportImpl::sendMessage(const WebrtcSignalingMessage& message) {
  try {
    nlohmann::json jsonMsg;
    if (message.isDescription()) {
      auto desc = message.getDescription();
      jsonMsg = desc.toJson();
    } else if (message.isCandidate()) {
      auto cand = message.getCandidate();
      jsonMsg = cand.toJson();
    }
    auto signedMessage = signer_->signMessage(jsonMsg);
    channel_->sendMessage(signedMessage);
  } catch (std::exception& e) {
    NPLOGE << "Failed to sign the message with error: " << e.what();
    auto err = nabto::webrtc::SignalingError(
        nabto::webrtc::SignalingErrorCode::VERIFICATION_ERROR,
        "Could not sign the signaling message");
    handleError(err);
  }
}

void MessageTransportImpl::setupSigner(const nlohmann::json& msg) {
  if (mode_ == SigningMode::NONE) {
    signer_ = NoneMessageSigner::create();
  } else if (mode_ == SigningMode::SHARED_SECRET) {
    auto keyId = SharedSecretMessageSigner::getKeyId(msg);
    auto secret = sharedSecretHandler_(keyId);
    signer_ = SharedSecretMessageSigner::create(secret, keyId);
  }
}

void MessageTransportImpl::requestIceServers() {
  auto self = shared_from_this();
  device_->requestIceServers(
      [self](const std::vector<struct nabto::webrtc::IceServer>& servers) {
        self->sendSetupResponse(servers);
        std::map<SetupDoneListenerId, SetupDoneHandler> setupHandlers;
        {
          const std::lock_guard<std::mutex> lock(self->handlerLock_);
          setupHandlers = self->setupHandlers_;
        }
        for (const auto& [id, handler] : setupHandlers) {
          handler(servers);
        }
      });
}

void MessageTransportImpl::sendSetupResponse(
    const std::vector<struct nabto::webrtc::IceServer>& iceServers) {
  nlohmann::json root;
  root["type"] = "SETUP_RESPONSE";
  root["iceServers"] = nlohmann::json::array();
  for (const auto& iceServer : iceServers) {
    nlohmann::json is;
    nlohmann::json urls = nlohmann::json::array();

    for (const auto& url : iceServer.urls) {
      urls.push_back(nlohmann::json::string_t(url));
    }

    is["urls"] = urls;
    if (!iceServer.credential.empty()) {
      is["credential"] = iceServer.credential;
    }
    if (!iceServer.username.empty()) {
      is["username"] = iceServer.username;
    }
    root["iceServers"].push_back(is);
  }
  auto signedMessage = signer_->signMessage(root);
  channel_->sendMessage(signedMessage);
}

void MessageTransportImpl::handleError(
    const nabto::webrtc::SignalingError& err) {
  channel_->sendError(err);
  std::map<TransportErrorListenerId, nabto::webrtc::SignalingErrorHandler>
      errHandlers;
  {
    const std::lock_guard<std::mutex> lock(handlerLock_);
    errHandlers = errHandlers_;
  }

  for (const auto& [id, handler] : errHandlers) {
    handler(err);
  }
}

}  // namespace util
}  // namespace webrtc
}  // namespace nabto
