#include "message_transport_impl.hpp"

#include <nabto/webrtc/util/message_transport.hpp>

namespace nabto {
namespace util {

MessageTransportPtr MessageTransportFactory::createSharedSecretTransport(
    signaling::SignalingDevicePtr device, signaling::SignalingChannelPtr sig,
    std::function<std::string(const std::string keyId)> handler) {
  return MessageTransportImpl::createSharedSecret(device, sig, handler);
}

MessageTransportPtr MessageTransportFactory::createNoneTransport(
    signaling::SignalingDevicePtr device, signaling::SignalingChannelPtr sig) {
  return MessageTransportImpl::createNone(device, sig);
}

}  // namespace util
}  // namespace nabto
