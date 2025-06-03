#pragma once

#include <nabto/signaling/signaling.hpp>

#include <nlohmann/json.hpp>

#include <cstdint>
#include <memory>

namespace nabto {
namespace signaling {

class SignalingDeviceImpl;
using SignalingDeviceImplPtr = std::shared_ptr<SignalingDeviceImpl>;

class SignalingChannelImpl;
using SignalingChannelImplPtr = std::shared_ptr<SignalingChannelImpl>;

/**
 * Signaling message types used in the SDK when talking to the backend
 */
enum SignalingMessageType : std::uint8_t {
  MESSAGE,
  ERROR,
  PEER_OFFLINE,
  PEER_CONNECTED,
  PING,
  PONG,
};

}  // namespace signaling
}  // namespace nabto
