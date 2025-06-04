#include <nabto/signaling/signaling.hpp>

#include <string>

namespace nabto {
namespace signaling {

std::string signalingDeviceStateToString(SignalingDeviceState state) {
  switch (state) {
    case SignalingDeviceState::NEW:
      return "NEW";
    case SignalingDeviceState::CONNECTING:
      return "CONNECTING";
    case SignalingDeviceState::CONNECTED:
      return "CONNECTED";
    case SignalingDeviceState::WAIT_RETRY:
      return "WAIT_RETRY";
    case SignalingDeviceState::FAILED:
      return "FAILED";
    case SignalingDeviceState::CLOSED:
      return "CLOSED";
    default:
      return "UNKNOWN";
  }
}

std::string signalingChannelStateToString(SignalingChannelState state) {
  switch (state) {
    case SignalingChannelState::NEW:
      return "NEW";
    case SignalingChannelState::ONLINE:
      return "ONLINE";
    case SignalingChannelState::OFFLINE:
      return "OFFLINE";
    case SignalingChannelState::FAILED:
      return "FAILED";
    case SignalingChannelState::CLOSED:
      return "CLOSED";
    default:
      return "UNKNOWN";
  }
}

}  // namespace signaling
}  // namespace nabto
