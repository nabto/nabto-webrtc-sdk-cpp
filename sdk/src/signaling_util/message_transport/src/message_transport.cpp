#include "message_transport_impl.hpp"

#include <nabto/webrtc/device.hpp>
#include <nabto/webrtc/util/message_transport.hpp>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace nabto {
namespace webrtc {
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

// SIGNALING DESCRIPTION START
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
SignalingDescription::SignalingDescription(std::string descType,
                                           std::string descSdp)
    : type(std::move(descType)), sdp(std::move(descSdp)) {}

nlohmann::json SignalingDescription::toJson() {
  nlohmann::json desc = {{"type", type}, {"sdp", sdp}};

  nlohmann::json msg = {
      {"type", "DESCRIPTION"},
      {"description", desc},
  };
  return msg;
}

// SIGNALING CANDIDATE START
SignalingCandidate::SignalingCandidate(std::string cand)
    : candidate(std::move(cand)) {}

void SignalingCandidate::setSdpMid(const std::string& mid) { sdpMid = mid; }
void SignalingCandidate::setSdpMLineIndex(int index) { sdpMLineIndex = index; }
void SignalingCandidate::setUsernameFragment(const std::string& ufrag) {
  usernameFragment = ufrag;
}

nlohmann::json SignalingCandidate::toJson() {
  nlohmann::json cand = {{"candidate", candidate}};
  if (!sdpMid.empty()) {
    cand["sdpMid"] = sdpMid;
  }

  if (sdpMLineIndex >= 0) {
    cand["sdpMLineIndex"] = sdpMLineIndex;
  }

  if (!usernameFragment.empty()) {
    cand["usernameFragment"] = usernameFragment;
  }
  nlohmann::json msg = {
      {"type", "CANDIDATE"},
      {"candidate", cand},
  };
  return msg;
}

// SIGNALING MESSAGE START

WebrtcSignalingMessage::WebrtcSignalingMessage(
    const SignalingDescription& desc) {
  description_ = std::make_unique<SignalingDescription>(desc);
}
WebrtcSignalingMessage::WebrtcSignalingMessage(const SignalingCandidate& cand) {
  candidate_ = std::make_unique<SignalingCandidate>(cand);
}

bool WebrtcSignalingMessage::isDescription() const {
  return description_ != nullptr;
}

bool WebrtcSignalingMessage::isCandidate() const {
  return candidate_ != nullptr;
}

SignalingDescription WebrtcSignalingMessage::getDescription() const {
  return *description_;
}
SignalingCandidate WebrtcSignalingMessage::getCandidate() const {
  return *candidate_;
}

WebrtcSignalingMessage WebrtcSignalingMessage::fromJson(
    nlohmann::json& jsonMessage) {
  auto type = jsonMessage.at("type").get<std::string>();
  if (type == "DESCRIPTION") {
    auto descType = jsonMessage.at("description").at("type").get<std::string>();
    auto sdp = jsonMessage.at("description").at("sdp").get<std::string>();
    const SignalingDescription desc(descType, sdp);
    return WebrtcSignalingMessage(desc);
  }
  if (type == "CANDIDATE") {
    auto candStr =
        jsonMessage.at("candidate").at("candidate").get<std::string>();
    SignalingCandidate cand(candStr);
    if (jsonMessage.at("candidate").contains("sdpMid")) {
      auto mid = jsonMessage.at("candidate").at("sdpMid").get<std::string>();
      cand.setSdpMid(mid);
    }
    if (jsonMessage.at("candidate").contains("sdpMLineIndex")) {
      auto index = jsonMessage.at("candidate").at("sdpMLineIndex").get<int>();
      cand.setSdpMLineIndex(index);
    }
    if (jsonMessage.at("candidate").contains("usernameFragment")) {
      auto ufrag =
          jsonMessage.at("candidate").at("usernameFragment").get<std::string>();
      cand.setUsernameFragment(ufrag);
    }
    return WebrtcSignalingMessage(cand);
  }
  throw std::runtime_error("invalid message type");
}

}  // namespace util
}  // namespace webrtc
}  // namespace nabto
