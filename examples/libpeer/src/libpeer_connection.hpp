#pragma once
#include <memory>
#include <mutex>
#include <peer.h>
#include <nabto/webrtc/device.hpp>
#include <nabto/webrtc/util/message_transport.hpp>

namespace nabto::example {

class WebrtcConnection;
typedef std::shared_ptr<WebrtcConnection> PeerConnectionPtr;

class WebrtcConnection : public std::enable_shared_from_this<WebrtcConnection> {
 public:
  static PeerConnectionPtr create(
      nabto::webrtc::SignalingDevicePtr device,
      nabto::webrtc::SignalingChannelPtr channel,
      nabto::webrtc::util::MessageTransportPtr transport);

  WebrtcConnection(nabto::webrtc::SignalingDevicePtr device,
                 nabto::webrtc::SignalingChannelPtr channel,
                 nabto::webrtc::util::MessageTransportPtr transport);

 private:
  static std::atomic<bool> isLibPeerInitialized_;

  static void onIceConnectionStateChange(PeerConnectionState state, void* userdata);
  static void onIceCandidate(char* description, void* userdata);
  static void onMessage(char* msg, size_t len, void* userdata, uint16_t sid);

  void init();
  void connectionLoop();
  void createPeerConnection();

  void sendDescription(const char* desc, SdpType type);
  void sendSignalingMessage(const nabto::webrtc::util::WebrtcSignalingMessage& message);

  void handleMessage(nabto::webrtc::util::WebrtcSignalingMessage& msg);
  void handleTransportError(const nabto::webrtc::SignalingError& error);
  void handleChannelStateChange(const nabto::webrtc::SignalingChannelState& state);
  void handleChannelError(const nabto::webrtc::SignalingError& error);

  nabto::webrtc::SignalingChannelPtr channel_;
  nabto::webrtc::SignalingDevicePtr device_;
  nabto::webrtc::util::MessageTransportPtr transport_;

  PeerConnection* pc_;
  PeerConnectionState pcState_;
  PeerConfiguration pcConfig_;
  bool running;

  std::thread connectionTaskThread_;
  std::mutex mutex_;
};
}  // namespace nabto::example
