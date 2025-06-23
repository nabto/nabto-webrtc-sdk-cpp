#include "message_transport_impl.hpp"

#include <nabto/webrtc/device.hpp>
#include <nabto/webrtc/util/message_transport.hpp>

#include <utility>

namespace nabto {
namespace util {

MessageTransportPtr MessageTransportFactory::createSharedSecretTransport(
    signaling::SignalingDevicePtr device,
    signaling::SignalingChannelPtr channel,
    MessageTransportSharedSecretHandler handler) {
  return MessageTransportImpl::createSharedSecret(
      std::move(device), std::move(channel), std::move(handler));
}

MessageTransportPtr MessageTransportFactory::createNoneTransport(
    signaling::SignalingDevicePtr device,
    signaling::SignalingChannelPtr channel) {
  return MessageTransportImpl::createNone(std::move(device),
                                          std::move(channel));
}

}  // namespace util
}  // namespace nabto
