#include <nabto/signaling/signaling.hpp>

namespace nabto {
namespace signaling {

SignalingError::SignalingError(SignalingErrorCode code, std::string message) :
    errorCode_(errorCodeToString(code)), errorMessage_(std::move(message))
{
}

SignalingError::SignalingError(std::string code, std::string message) :
    errorCode_(std::move(code)), errorMessage_(std::move(message))
{
}

std::string SignalingError::errorCodeToString(SignalingErrorCode code)
{
    switch (code) {
    case SignalingErrorCode::CHANNEL_CLOSED: return "CHANNEL_CLOSED";
    case SignalingErrorCode::CHANNEL_NOT_FOUND: return "CHANNEL_NOT_FOUND";
    default: return "UNKNOWN";
}
}


} } // namespaces
