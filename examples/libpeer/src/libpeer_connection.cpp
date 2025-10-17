#include "libpeer_connection.hpp"

#include <nabto/webrtc/util/logging.hpp>

namespace nabto::example {

namespace nrtc = nabto::webrtc;

std::atomic<bool> WebrtcConnection::isLibPeerInitialized_ = false;

PeerConnectionPtr WebrtcConnection::create(
    nrtc::SignalingDevicePtr device, nrtc::SignalingChannelPtr channel,
    nrtc::util::MessageTransportPtr transport) {
  if (!isLibPeerInitialized_) {
    isLibPeerInitialized_ = true;
    peer_init();
  }

  auto conn = std::make_shared<WebrtcConnection>(device, channel, transport);
  conn->init();
  conn->pcConfig_ = {};
  return conn;
}

WebrtcConnection::WebrtcConnection(nrtc::SignalingDevicePtr device,
                                   nrtc::SignalingChannelPtr channel,
                                   nrtc::util::MessageTransportPtr transport)
    : device_(device), channel_(channel), transport_(transport) {}

void WebrtcConnection::init() {
  auto self = shared_from_this();

  transport_->addMessageListener(
      [self](nrtc::util::WebrtcSignalingMessage& msg) {
        NPLOGI << "Webrtc signaling message received";
        self->handleMessage(msg);
      });

  transport_->addSetupDoneListener(
      [self](const std::vector<nrtc::IceServer>& iceServers) {
        NPLOGI << "Transport setup DONE";
        int n = 0;
        for (auto& iceServer : iceServers) {
          auto pcIceServer = &self->pcConfig_.ice_servers[n++];
          pcIceServer->urls = iceServer.urls[0].c_str();
          pcIceServer->username = iceServer.username.c_str();
          pcIceServer->credential = iceServer.credential.c_str();
        }
        self->createPeerConnection();
      });

  transport_->addErrorListener([self](const nrtc::SignalingError& error) {
    NPLOGE << "Nabto signaling error: " << error.errorCode();
    self->handleTransportError(error);
  });

  channel_->addStateChangeListener([self](nrtc::SignalingChannelState event) {
    NPLOGI << "SignalingChannelState changed";
    self->handleChannelStateChange(event);
  });

  channel_->addErrorListener([self](const nrtc::SignalingError& error) {
    NPLOGE << "Channel error: " << error.errorCode();
    self->handleChannelError(error);
  });
}

void WebrtcConnection::createPeerConnection() {
  NPLOGI << "Creating LibPeer connection...";

  pcConfig_.datachannel = DATA_CHANNEL_NONE;
  pcConfig_.video_codec = CODEC_H264;
  pcConfig_.audio_codec = CODEC_NONE;
  pcConfig_.user_data = this;

  pc_ = peer_connection_create(&pcConfig_);
  peer_connection_oniceconnectionstatechange(pc_, &WebrtcConnection::onIceConnectionStateChange);
  peer_connection_onicecandidate(pc_, &WebrtcConnection::onIceCandidate);

  const char* offer = peer_connection_create_offer(pc_);
  NPLOGI << "OFFER: " << offer;
  sendDescription(offer, SDP_TYPE_OFFER);

  if (pc_) {
    running = true;
    connectionTaskThread_ = std::thread(&WebrtcConnection::connectionLoop, this);
  }
}

void WebrtcConnection::connectionLoop() {
  while (running) {
    peer_connection_loop(pc_);
    usleep(1000);
  }
}

void WebrtcConnection::sendDescription(const char* description, SdpType type) {
  std::string typeString = type == SDP_TYPE_ANSWER ? "answer" : "offer";
  if (description) {
    nrtc::util::SignalingDescription desc(typeString, std::string(description));
    sendSignalingMessage(nrtc::util::WebrtcSignalingMessage(desc));
  }
}

void WebrtcConnection::sendSignalingMessage(const nrtc::util::WebrtcSignalingMessage& message) {
  if (!transport_) {
    return;
  }

  try {
    transport_->sendMessage(message);
  } catch (std::exception& e) {
    NPLOGE << "Failed to sign the message with error: " << e.what();
  }
}

void WebrtcConnection::handleMessage(nrtc::util::WebrtcSignalingMessage& msg) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (msg.isDescription()) {
    auto desc = msg.getDescription();
    NPLOGI << "Description received: " << desc.sdp;
  }
}

void WebrtcConnection::handleTransportError(const nrtc::SignalingError& error) {

}

void WebrtcConnection::handleChannelStateChange(
    const nrtc::SignalingChannelState& state) {}

void WebrtcConnection::handleChannelError(const nrtc::SignalingError& error) {}

void WebrtcConnection::onIceConnectionStateChange(PeerConnectionState state,
                                                  void* userdata) {
  NPLOGI << "ICE Connection state changed to " << state;
}

void WebrtcConnection::onIceCandidate(char* description, void* userdata) {

}

}  // namespace nabto::example