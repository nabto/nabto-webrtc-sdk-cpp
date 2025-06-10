#include "message_transport_impl.hpp"

#include "message_signer.hpp"
#include "none_message_signer.hpp"
#include "shared_secret_message_signer.hpp"

namespace nabto {
namespace util {

MessageTransportPtr MessageTransportImpl::create(
    signaling::SignalingDevicePtr device, signaling::SignalingChannelPtr sig,
    SecurityMode mode) {
  return std::make_shared<MessageTransportImpl>(device, sig, mode);
}

MessageTransportImpl::MessageTransportImpl(signaling::SignalingDevicePtr device,
                                           signaling::SignalingChannelPtr sig,
                                           SecurityMode mode)
    : device_(device), sig_(sig) {
  if (mode == SecurityMode::NONE) {
    signer_ = NoneMessageSigner::create();
  } else if (mode == SecurityMode::SHARED_SECRET) {
    std::string sec = "foo";
    std::string secId = "bar";
    signer_ = SharedSecretMessageSigner::create(sec, secId);  // TODO: fix this
  }
}

void MessageTransportImpl::setSharedSecretHandler(
    std::function<std::string(const std::string keyId)> handler) {
  secretHandler_ = handler;
}

void MessageTransportImpl::setSetupDoneHandler(
    std::function<void(const std::vector<rtc::IceServer>& iceServers)>
        handler) {
  setupHandler_ = handler;
}

/**
 * Set a handler to be invoked whenever a message is available on the
 * connection
 *
 * @param handler the handler to set
 */
void MessageTransportImpl::setMessageHandler(
    signaling::SignalingMessageHandler handler) {
  msgHandler_ = handler;
}

/**
 * Set a handler to be invoked if an error occurs on the connection
 *
 * @param handler the handler to set
 */
void MessageTransportImpl::setErrorHandler(
    signaling::SignalingErrorHandler handler) {
  errHandler_ = handler;
}

void MessageTransportImpl::sendMessage(const nlohmann::json& message) {}

}  // namespace util
}  // namespace nabto
