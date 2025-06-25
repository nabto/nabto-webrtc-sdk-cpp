#include <nabto/webrtc/device.hpp>

#include <string>
#include <utility>

namespace nabto {
namespace webrtc {

SignalingError::SignalingError(SignalingErrorCode code, std::string message)
    : errorCode_(errorCodeToString(code)), errorMessage_(std::move(message)) {}

SignalingError::SignalingError(std::string code, std::string message)
    : errorCode_(std::move(code)), errorMessage_(std::move(message)) {}

std::string SignalingError::errorCodeToString(SignalingErrorCode code) {
  switch (code) {
    case SignalingErrorCode::DECODE_ERROR:
      return "DECODE_ERROR";
    case SignalingErrorCode::VERIFICATION_ERROR:
      return "VERIFICATION_ERROR";
    case SignalingErrorCode::CHANNEL_CLOSED:
      return "CHANNEL_CLOSED";
    case SignalingErrorCode::CHANNEL_NOT_FOUND:
      return "CHANNEL_NOT_FOUND";
    case SignalingErrorCode::NO_MORE_CHANNELS:
      return "NO_MORE_CHANNELS";
    case SignalingErrorCode::ACCESS_DENIED:
      return "ACCESS_DENIED";
    case SignalingErrorCode::INTERNAL_ERROR:
      return "INTERNAL_ERROR";
    default:
      return "UNKNOWN";
  }
}

}  // namespace webrtc
}  // namespace nabto
