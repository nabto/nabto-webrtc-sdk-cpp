#include <nabto/signaling/signaling.hpp>

namespace nabto {
namespace signaling {

std::string signalingDeviceStateToString(SignalingDeviceState state)
{
    switch (state)
    {
    case SignalingDeviceState::NEW: return "NEW";
    case SignalingDeviceState::CONNECTING: return "CONNECTING";
    case SignalingDeviceState::CONNECTED: return "CONNECTED";
    case SignalingDeviceState::WAIT_RETRY: return "WAIT_RETRY";
    case SignalingDeviceState::FAILED: return "FAILED";
    case SignalingDeviceState::CLOSED: return "CLOSED";
    default: return "UNKNOWN";
    }
}

std::string signalingChannelStateToString(SignalingChannelState state)
{
    switch (state)
    {
    case SignalingChannelState::NEW: return "NEW";
    case SignalingChannelState::ONLINE: return "ONLINE";
    case SignalingChannelState::OFFLINE: return "OFFLINE";
    case SignalingChannelState::FAILED: return "FAILED";
    case SignalingChannelState::CLOSED: return "CLOSED";
    default: return "UNKNOWN";
    }
}

/**
 * converts the SignalingError object to json format:
 * {
 *  code: ...,
 *  message: ...
 * }
 */
nlohmann::json signalingErrorToJson(const SignalingError& err)
{
    nlohmann::json error = {
        {"code", err.errorCode()},
        {"message", err.errorMessage()}
    };
    return error;
}

/**
 * Creates a SignalingError object from the json format:
 * {
 *   code: ...,
 *   message: ...
 * }
 */
SignalingError signalingErrorFromJson(const nlohmann::json& err)
{
    std::string msg;
    if (err.contains("message")) {
        msg = err["message"].get<std::string>();
    }

    return SignalingError(err["code"].get<std::string>(), msg);
}

}
} // namespaces
