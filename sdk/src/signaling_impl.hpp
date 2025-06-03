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

/**
 * converts the SignalingError object to json format:
 * {
 *  code: ...,
 *  message: ...
 * }
 */
nlohmann::json signalingErrorToJson(const SignalingError& err);

/**
 * Creates a SignalingError object from the json format:
 * {
 *   code: ...,
 *   message: ...
 * }
 */
SignalingError signalingErrorFromJson(const nlohmann::json& err);

}  // namespace signaling
}  // namespace nabto
