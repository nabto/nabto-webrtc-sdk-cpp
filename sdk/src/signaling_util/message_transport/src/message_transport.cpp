#include "message_transport_impl.hpp"

#include <nabto/webrtc/device.hpp>
#include <nabto/webrtc/util/message_transport.hpp>

#include <utility>

namespace nabto {
namespace util {

MessageTransportPtr MessageTransportFactory::createSharedSecretTransport(
    nabto::webrtc::SignalingDevicePtr device,
    nabto::webrtc::SignalingChannelPtr channel,
    MessageTransportSharedSecretHandler handler) {
  return MessageTransportImpl::createSharedSecret(
      std::move(device), std::move(channel), std::move(handler));
}

MessageTransportPtr MessageTransportFactory::createNoneTransport(
    nabto::webrtc::SignalingDevicePtr device,
    nabto::webrtc::SignalingChannelPtr channel) {
  return MessageTransportImpl::createNone(std::move(device),
                                          std::move(channel));
}

}  // namespace util
}  // namespace nabto
