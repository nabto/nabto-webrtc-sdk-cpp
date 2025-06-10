#include "message_transport_impl.hpp"

namespace nabto {
namespace util {

MessageTransportPtr MessageTransportImpl::create(
    signaling::SignalingDevicePtr device, signaling::SignalingChannelPtr sig,
    SecurityMode mode) {
  return std::make_shared<MessageTransportImpl>(device, sig, mode);
}

MessageTransportImpl::MessageTransportImpl(signaling::SignalingDevicePtr device,
                                           signaling::SignalingChannelPtr sig,
                                           SecurityMode mode) {}

void MessageTransportImpl::setSharedSecretHandler(
    std::function<std::string(const std::string keyId)> callback) {}

void MessageTransportImpl::setSetupDoneHandler(
    std::function<void(const std::vector<rtc::IceServer>& iceServers)>
        callback) {}

/**
 * Set a handler to be invoked whenever a message is available on the
 * connection
 *
 * @param handler the handler to set
 */
void MessageTransportImpl::setMessageHandler(
    signaling::SignalingMessageHandler handler) {}

/**
 * Set a handler to be invoked if an error occurs on the connection
 *
 * @param handler the handler to set
 */
void MessageTransportImpl::setErrorHandler(
    signaling::SignalingErrorHandler handler) {}

void MessageTransportImpl::sendMessage(const nlohmann::json& message) {}

}  // namespace util
}  // namespace nabto
