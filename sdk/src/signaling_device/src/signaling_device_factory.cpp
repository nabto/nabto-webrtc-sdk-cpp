#include "signaling_device_impl.hpp"

#include <nabto/webrtc/device.hpp>

namespace nabto {
namespace webrtc {

SignalingDevicePtr SignalingDeviceFactory::create(
    const SignalingDeviceConfig& conf) {
  return SignalingDeviceImpl::create(conf);
}
}  // namespace webrtc
}  // namespace nabto
