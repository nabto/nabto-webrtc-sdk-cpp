#pragma once
#include <memory>
#include <nabto/webrtc/device.hpp>
#include <nabto/webrtc/util/message_transport.hpp>
#include <rtc/rtc.hpp>

#include "track_handler.hpp"

namespace nabto {
namespace example {
class WebrtcConnection;
typedef std::shared_ptr<WebrtcConnection> WebrtcConnectionPtr;

class WebrtcConnection : public std::enable_shared_from_this<WebrtcConnection> {
 public:
  static WebrtcConnectionPtr create(
      nabto::webrtc::SignalingDevicePtr device,
      nabto::webrtc::SignalingChannelPtr sig,
      nabto::util::MessageTransportPtr messageTransport,
      WebrtcTrackHandlerPtr trackHandler);
  WebrtcConnection(nabto::webrtc::SignalingDevicePtr device,
                   nabto::webrtc::SignalingChannelPtr channel,
                   nabto::util::MessageTransportPtr messageTransport,
                   WebrtcTrackHandlerPtr trackHandler);

  void handleMessage(const nlohmann::json& msg);

  // TODO: add this
  // void handleReconnect();

 private:
  nabto::webrtc::SignalingChannelPtr channel_;
  nabto::webrtc::SignalingDevicePtr device_;
  nabto::util::MessageTransportPtr messageTransport_;
  std::shared_ptr<rtc::PeerConnection> pc_ = nullptr;
  bool canTrickle_ = true;
  bool ignoreOffer_ = false;
  bool polite_;
  bool hasIce_ = false;
  WebrtcTrackHandlerPtr videoTrack_;
  std::vector<rtc::IceServer> iceServers_;
  size_t trackRef_ = 0;

  std::vector<nlohmann::json> messageQueue_;

  void init();
  void deinit();
  void createPeerConnection();
  void handleStateChange(const rtc::PeerConnection::State& state);
  void handleSignalingStateChange(rtc::PeerConnection::SignalingState state);
  void handleLocalCandidate(rtc::Candidate cand);
  void handleTrackEvent(std::shared_ptr<rtc::Track> track);
  void handleDatachannelEvent(std::shared_ptr<rtc::DataChannel> channel);
  void handleGatherinsStateChange(rtc::PeerConnection::GatheringState state);

  void sendDescription(rtc::optional<rtc::Description> description);
  void sendCreateResponse(
      const std::vector<struct nabto::webrtc::IceServer>& iceServers);
  void requestIceServers();
  void parseIceServers(
      const std::vector<struct nabto::webrtc::IceServer>& servers);

  void sendSignalingMessage(const nlohmann::json& message);

  void addTrack();

  size_t datachannelSendSeq_ = 0;
};

}  // namespace example
}  // namespace nabto
