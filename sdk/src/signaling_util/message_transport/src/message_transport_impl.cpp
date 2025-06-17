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
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace nabto {
namespace util {

MessageTransportPtr MessageTransportImpl::createNone(
    signaling::SignalingDevicePtr device,
    signaling::SignalingChannelPtr channel) {
  auto ptr = std::make_shared<MessageTransportImpl>(std::move(device),
                                                    std::move(channel));
  if (ptr) {
    ptr->init();
  }
  return ptr;
}

MessageTransportPtr MessageTransportImpl::createSharedSecret(
    signaling::SignalingDevicePtr device,
    signaling::SignalingChannelPtr channel,
    std::function<std::string(const std::string keyId)> sharedSecretHandler) {
  auto ptr = std::make_shared<MessageTransportImpl>(
      std::move(device), std::move(channel), std::move(sharedSecretHandler));
  if (ptr) {
    ptr->init();
  }
  return ptr;
}

MessageTransportImpl::MessageTransportImpl(
    signaling::SignalingDevicePtr device,
    signaling::SignalingChannelPtr channel)
    : device_(std::move(device)),
      channel_(std::move(channel)),
      mode_(SigningMode::NONE) {}

MessageTransportImpl::MessageTransportImpl(
    signaling::SignalingDevicePtr device,
    signaling::SignalingChannelPtr channel,
    std::function<std::string(const std::string keyId)> sharedSecretHandler)
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
  if (!signer_) {
    setupSigner(msgIn);
  }
  try {
    NPLOGI << "Webrtc got signaling message IN: " << msgIn.dump();
    auto msg = signer_->verifyMessage(msgIn);

    NPLOGI << "Webrtc got signaling message: " << msg.dump();
    auto type = msg.at("type").get<std::string>();
    if (type == "SETUP_REQUEST") {
      requestIceServers();
    } else if (type == "DESCRIPTION" || type == "CANDIDATE") {
      if (msgHandlers_.size() == 0) {
        NPLOGE << "Received signaling message without a registered message "
                  "handler";
      } else {
        for (const auto& [id, handler] : msgHandlers_) {
          handler(msg);
        }
      }
    }
  } catch (nabto::util::VerificationError& ex) {
    NPLOGE << "Could not verify the incoming signaling message: "
           << msgIn.dump() << " with: " << ex.what();
    auto err = nabto::signaling::SignalingError(
        nabto::signaling::SignalingErrorCode::VERIFICATION_ERROR,
        "Could not verify the incoming signaling message");
    handleError(err);
  } catch (nabto::util::DecodeError& ex) {
    NPLOGE << "Could not decode the incoming signaling message: "
           << msgIn.dump() << " with: " << ex.what();
    auto err = nabto::signaling::SignalingError(
        nabto::signaling::SignalingErrorCode::DECODE_ERROR,
        "Could not decode the incoming signaling message");
    handleError(err);
  } catch (nlohmann::json::exception& ex) {
    NPLOGE << "Failed to parse json: " << ex.what()
           << " for msg: " << msgIn.dump();
    auto err = nabto::signaling::SignalingError(
        nabto::signaling::SignalingErrorCode::DECODE_ERROR,
        "Could not decode the incoming signaling message");
    handleError(err);

  } catch (std::exception& ex) {
    NPLOGE << "Failed to handle message: " << msgIn.dump()
           << " with: " << ex.what();
    auto err = nabto::signaling::SignalingError(
        nabto::signaling::SignalingErrorCode::INTERNAL_ERROR, "Internal error");
    handleError(err);
  }
};

SetupDoneListenerId MessageTransportImpl::addSetupDoneListener(
    std::function<void(const std::vector<signaling::IceServer>& iceServers)>
        handler) {
  const SetupDoneListenerId id = currSetupListId_;
  currSetupListId_++;
  setupHandlers_.insert({id, handler});
  return id;
}

TransportMessageListenerId MessageTransportImpl::addMessageListener(
    signaling::SignalingMessageHandler handler) {
  const TransportMessageListenerId id = currMsgListId_;
  currMsgListId_++;
  msgHandlers_.insert({id, handler});
  return id;
}

TransportErrorListenerId MessageTransportImpl::addErrorListener(
    signaling::SignalingErrorHandler handler) {
  const TransportErrorListenerId id = currErrListId_;
  currErrListId_++;
  errHandlers_.insert({id, handler});
  return id;
}

void MessageTransportImpl::sendMessage(const nlohmann::json& message) {
  try {
    auto signedMessage = signer_->signMessage(message);
    channel_->sendMessage(signedMessage);
  } catch (std::exception& e) {
    NPLOGE << "Failed to sign the message: " << message.dump()
           << ", error: " << e.what();
    // TODO(tk): handle exception
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
  // TODO(tk): if something fails here handle the error
}

void MessageTransportImpl::requestIceServers() {
  auto self = shared_from_this();
  device_->requestIceServers(
      [self](const std::vector<struct nabto::signaling::IceServer>& servers) {
        self->sendSetupResponse(servers);
        for (const auto& [id, handler] : self->setupHandlers_) {
          handler(servers);
        }
      });
}

void MessageTransportImpl::sendSetupResponse(
    const std::vector<struct nabto::signaling::IceServer>& iceServers) {
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
  sendMessage(root);
}

void MessageTransportImpl::handleError(const signaling::SignalingError& err) {
  channel_->sendError(err);
  for (const auto& [id, handler] : errHandlers_) {
    handler(err);
  }
}

}  // namespace util
}  // namespace nabto
