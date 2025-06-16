#include "message_transport_impl.hpp"

#include <nabto/webrtc/device.hpp>
#include <nabto/webrtc/util/message_transport.hpp>

#include <functional>
#include <string>
#include <utility>

namespace nabto {
namespace util {

MessageTransportPtr MessageTransportFactory::createSharedSecretTransport(
    signaling::SignalingDevicePtr device, signaling::SignalingChannelPtr sig,
    std::function<std::string(const std::string keyId)> handler) {
  return MessageTransportImpl::createSharedSecret(
      std::move(device), std::move(sig), std::move(handler));
}

MessageTransportPtr MessageTransportFactory::createNoneTransport(
    signaling::SignalingDevicePtr device, signaling::SignalingChannelPtr sig) {
  return MessageTransportImpl::createNone(std::move(device), std::move(sig));
}

}  // namespace util
}  // namespace nabto
