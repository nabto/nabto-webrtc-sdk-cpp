#include "message_transport_impl.hpp"

#include <nabto/webrtc/util/message_transport.hpp>

namespace nabto {
namespace util {
MessageTransportPtr MessageTransportFactory::create(
    signaling::SignalingDevicePtr device, signaling::SignalingChannelPtr sig,
    SecurityMode mode) {}

}  // namespace util
}  // namespace nabto
