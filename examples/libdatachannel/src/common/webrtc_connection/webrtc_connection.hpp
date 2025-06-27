#pragma once
#include <memory>
#include <mutex>
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
      nabto::webrtc::util::MessageTransportPtr messageTransport,
      WebrtcTrackHandlerPtr trackHandler);
  WebrtcConnection(nabto::webrtc::SignalingDevicePtr device,
                   nabto::webrtc::SignalingChannelPtr channel,
                   nabto::webrtc::util::MessageTransportPtr messageTransport,
                   WebrtcTrackHandlerPtr trackHandler);

  // TODO: add this
  // void handleReconnect();

 private:
  nabto::webrtc::SignalingChannelPtr channel_;
  nabto::webrtc::SignalingDevicePtr device_;
  nabto::webrtc::util::MessageTransportPtr messageTransport_;
  std::shared_ptr<rtc::PeerConnection> pc_ = nullptr;
  bool canTrickle_ = true;
  bool ignoreOffer_ = false;
  bool polite_;
  bool hasIce_ = false;
  WebrtcTrackHandlerPtr videoTrack_;
  std::vector<rtc::IceServer> iceServers_;
  size_t trackRef_ = 0;
  std::mutex mutex_;

  std::vector<nlohmann::json> messageQueue_;

  void handleMessage(nabto::webrtc::util::WebrtcSignalingMessage& msg);
  void parseIceServers(
      const std::vector<struct nabto::webrtc::IceServer>& servers);
  void handleTransportError(const nabto::webrtc::SignalingError& error);
  void handleChannelStateChange(
      const nabto::webrtc::SignalingChannelState& state);
  void handleChannelError(const nabto::webrtc::SignalingError& error);
  void handleStateChange(const rtc::PeerConnection::State& state);
  void handleSignalingStateChange(rtc::PeerConnection::SignalingState state);
  void handleLocalCandidate(rtc::Candidate cand);
  void handleTrackEvent(std::shared_ptr<rtc::Track> track);
  void handleDatachannelEvent(std::shared_ptr<rtc::DataChannel> channel);
  void handleGatherinsStateChange(rtc::PeerConnection::GatheringState state);

  void init();
  void deinit();
  void createPeerConnection();

  void sendDescription(rtc::optional<rtc::Description> description);
  void sendCreateResponse(
      const std::vector<struct nabto::webrtc::IceServer>& iceServers);
  void requestIceServers();

  void sendSignalingMessage(
      const nabto::webrtc::util::WebrtcSignalingMessage& message);

  void addTrack();

  size_t datachannelSendSeq_ = 0;
};

}  // namespace example
}  // namespace nabto
