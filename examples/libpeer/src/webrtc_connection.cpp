#include "webrtc_connection.hpp"

#include <nabto/webrtc/util/logging.hpp>
#include <nlohmann/json.hpp>

namespace nabto {
namespace example {

WebrtcConnectionPtr WebrtcConnection::create(
    nabto::webrtc::SignalingDevicePtr device,
    nabto::webrtc::SignalingChannelPtr channel,
    nabto::webrtc::util::MessageTransportPtr messageTransport,
    WebrtcTrackHandlerPtr trackHandler) {
  auto p = std::make_shared<WebrtcConnection>(device, channel, messageTransport,
                                              trackHandler);
  p->init();
  return p;
}

WebrtcConnection::WebrtcConnection(
    nabto::webrtc::SignalingDevicePtr device,
    nabto::webrtc::SignalingChannelPtr channel,
    nabto::webrtc::util::MessageTransportPtr messageTransport,
    WebrtcTrackHandlerPtr trackHandler)
    : device_(device),
      channel_(channel),
      messageTransport_(messageTransport),
      videoTrack_(trackHandler),
      polite_(true) {}

void WebrtcConnection::init() {
  // createPeerConnection();
  auto self = shared_from_this();

  messageTransport_->addMessageListener(
      [self](nabto::webrtc::util::WebrtcSignalingMessage& msg) {
        self->handleMessage(msg);
      });

  messageTransport_->addSetupDoneListener(
      [self](const std::vector<nabto::webrtc::IceServer>& iceServers) {
        self->parseIceServers(iceServers);
      });

  messageTransport_->addErrorListener(
      [self](const nabto::webrtc::SignalingError& error) {
        NPLOGE << "Got errorCode: " << error.errorCode();
        // All errors are fatal, so we clean up no matter what the error was
        self->handleTransportError(error);
      });

  channel_->addStateChangeListener(
      [self](nabto::webrtc::SignalingChannelState event) {
        self->handleChannelStateChange(event);
      });

  channel_->addErrorListener(
      [self](const nabto::webrtc::SignalingError& error) {
        NPLOGE << "Got errorCode: " << error.errorCode();
        // All errors are fatal, so we clean up no matter what the error was
        self->handleChannelError(error);
      });
}

void WebrtcConnection::handleMessage(
    nabto::webrtc::util::WebrtcSignalingMessage& msg) {
}

void WebrtcConnection::createPeerConnection() {
  auto self = shared_from_this();

}

void WebrtcConnection::addTrack() {
  const std::lock_guard<std::mutex> lock(mutex_);
}


void WebrtcConnection::sendSignalingMessage(
    const nabto::webrtc::util::WebrtcSignalingMessage& message) {
  if (!messageTransport_) {
    return;
  }
  try {
    messageTransport_->sendMessage(message);
  } catch (std::exception& e) {
    NPLOGE << "Failed to sign the message with error: " << e.what();
  }
}

void WebrtcConnection::parseIceServers(
    const std::vector<struct nabto::webrtc::IceServer>& servers) {
  for (auto s : servers) {
    std::string proto = "";
    if (s.username.empty()) {
      proto = "stun:";
    } else {
      proto = "turn:";
    }
    const std::lock_guard<std::mutex> lock(mutex_);
    for (auto u : s.urls) {
      std::stringstream ss;
      std::string host = u;
      if (host.rfind("turn:", 0) == 0 || host.rfind("stun:", 0) == 0) {
        host = host.substr(5);
      }
      ss << proto;
      if (!s.username.empty() && !s.credential.empty()) {
        std::string username = s.username;
        auto n = username.find(":");
        while (n != std::string::npos) {
          username.replace(n, 1, "%3A");
          n = username.find(":");
        }
        ss << username << ":" << s.credential << "@";
      }
      ss << host;

      /*
      auto server = rtc::IceServer(ss.str());
      NPLOGD << "Created server with hostname: " << server.hostname << std::endl
             << "    port: " << server.port << std::endl
             << "    username: " << server.username << std::endl
             << "    password: " << server.password << std::endl
             << "    type: "
             << (server.type == rtc::IceServer::Type::Turn ? "TURN" : "STUN")
             << std::endl
             << "    RelayType: "
             << (server.relayType == rtc::IceServer::RelayType::TurnUdp
                     ? "TurnUdp"
                     : (server.relayType == rtc::IceServer::RelayType::TurnTcp
                            ? "TurnTcp"
                            : "TurnTls"));
                          */
      //iceServers_.push_back(server);
    }
  }
  createPeerConnection();
  addTrack();
}

void WebrtcConnection::deinit() {
  std::shared_ptr<rtc::PeerConnection> pc;
  nabto::webrtc::SignalingChannelPtr chan;
  {
    const std::lock_guard<std::mutex> lock(mutex_);
    //pc = pc_;
    chan = channel_;

    if (trackRef_ != 0 && videoTrack_ != nullptr) {
      NPLOGE << "Remove conn from videotrack";
      videoTrack_->removeConnection(trackRef_);
    } else {
      NPLOGE << "deinit without video track: " << trackRef_;
    }
    videoTrack_ = nullptr;
    if (channel_) {
      channel_ = nullptr;
    }
    messageTransport_ = nullptr;
    // sig_ = nullptr;
  }
  if (chan) {
    chan->close();
  }
  //pc_ = nullptr;
}

void WebrtcConnection::handleTransportError(
    const nabto::webrtc::SignalingError& error) {
  std::shared_ptr<rtc::PeerConnection> pc = nullptr;
  {
    const std::lock_guard<std::mutex> lock(mutex_);
    //pc = pc_;
  }
  /*
  if (pc) {
    pc->close();
  } else {
    deinit();
  }*/
 deinit();
}

void WebrtcConnection::handleChannelError(
    const nabto::webrtc::SignalingError& error) {
  handleTransportError(error);
}

void WebrtcConnection::handleChannelStateChange(
    const nabto::webrtc::SignalingChannelState& state) {

  switch (state) {
    case nabto::webrtc::SignalingChannelState::DISCONNECTED:
      NPLOGD << "Got channel state: DISCONNECTED";
      // This means we tried to send a signaling message but the client is
      // not connected to the backend. If we expect the client to connect
      // momentarily, we will then get a CLIENT_CONNECTED event and the
      // reliability layer will fix it and we can ignore this event.
      // Otherwise we should handle the error.
      break;
    case nabto::webrtc::SignalingChannelState::CLOSED:
      NPLOGD << "Got channel state: CLOSED";
      {
        const std::lock_guard<std::mutex> lock(mutex_);
        //pc = pc_;
      }
      //if (pc) {
      
        //pc->close();
      //} else {
        deinit();
      //}
      break;
    case nabto::webrtc::SignalingChannelState::CONNECTED:
      NPLOGD << "Got channel state CONNECTED";
      // This means client reconnected, the client should do ICE restart
      // if needed so we can just ignore.
      break;
    case nabto::webrtc::SignalingChannelState::FAILED:
      NPLOGD << "Got channel state: FAILED";
      {
        const std::lock_guard<std::mutex> lock(mutex_);
        //pc = pc_;
      }
      /*
      if (pc) {
        pc->close();
      } else {
        deinit();
      }
      */
     deinit();
      break;
  }
}

}  // namespace example
}  // namespace nabto
