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
  mutex_.lock();
  try {
    NPLOGI << "Webrtc got signaling message";

    if (msg.isDescription()) {
      auto desc = msg.getDescription();
      rtc::Description remDesc(desc.sdp, desc.type);

      bool offerCollision =
          remDesc.type() == rtc::Description::Type::Offer &&
          pc_->signalingState() != rtc::PeerConnection::SignalingState::Stable;

      ignoreOffer_ = !polite_ && offerCollision;
      if (ignoreOffer_) {
        NPLOGD << "The device is impolite and there is a collision so we are "
                  "discarding the received offer";
        mutex_.unlock();
        return;
      }
      std::shared_ptr<rtc::PeerConnection> pc = pc_;
      mutex_.unlock();
      try {
        pc->setRemoteDescription(remDesc);
      } catch (std::exception& ex) {
        NPLOGE << "Failed to set remote description: " << remDesc.generateSdp()
               << " Failed with exception: " << ex.what();
        pc->close();
      }
      return;
    }
    if (msg.isCandidate()) {
      std::shared_ptr<rtc::PeerConnection> pc = pc_;
      mutex_.unlock();
      try {
        auto sigCand = msg.getCandidate();
        rtc::Candidate cand(sigCand.candidate, sigCand.sdpMid);
        pc->addRemoteCandidate(cand);
      } catch (nlohmann::json::exception& ex) {
        NPLOGE << "handleIce json exception: " << ex.what();
      } catch (std::logic_error& ex) {
        mutex_.lock();
        if (!this->ignoreOffer_) {
          NPLOGE << "Failed to add remote ICE candidate with logic error: "
                 << ex.what();
        }
        mutex_.unlock();
      }
      return;
    }

    NPLOGE << "Got unknown message type";
  } catch (std::exception& ex) {
    NPLOGE << "Failed to handle message with: " << ex.what();
  }
  mutex_.unlock();
}

void WebrtcConnection::createPeerConnection() {
  auto self = shared_from_this();
  rtc::Configuration conf;

  // conf.iceTransportPolicy = rtc::TransportPolicy::Relay;
  conf.disableAutoNegotiation = true;
  conf.forceMediaTransport = true;
  conf.iceServers = iceServers_;
  pc_ = std::make_shared<rtc::PeerConnection>(conf);

  pc_->onStateChange([self](const rtc::PeerConnection::State& state) {
    std::ostringstream oss;
    oss << state;
    NPLOGD << "State: " << oss.str();
    self->handleStateChange(state);
  });

  pc_->onSignalingStateChange(
      [self](rtc::PeerConnection::SignalingState state) {
        std::ostringstream oss;
        oss << state;
        NPLOGD << "Signalling State: " << oss.str();
        self->handleSignalingStateChange(state);
      });

  pc_->onLocalCandidate([self](rtc::Candidate cand) {
    NPLOGD << "Got local candidate: " << cand;
    self->handleLocalCandidate(cand);
  });

  pc_->onTrack([self](std::shared_ptr<rtc::Track> track) {
    NPLOGD << "Got Track event";
    self->handleTrackEvent(track);
  });

  pc_->onDataChannel([self](std::shared_ptr<rtc::DataChannel> incoming) {
    NPLOGD << "Got new datachannel: " << incoming->label();
    self->handleDatachannelEvent(incoming);
  });

  pc_->onGatheringStateChange(
      [self](rtc::PeerConnection::GatheringState state) {
        std::ostringstream oss;
        oss << state;
        NPLOGD << "Gathering State: " << oss.str();
        self->handleGatherinsStateChange(state);
      });
}

void WebrtcConnection::handleStateChange(
    const rtc::PeerConnection::State& state) {
  std::shared_ptr<rtc::PeerConnection> pc = nullptr;
  nabto::webrtc::SignalingChannelPtr channel = nullptr;

  switch (state) {
    case rtc::PeerConnection::State::New:
    case rtc::PeerConnection::State::Connecting:
    case rtc::PeerConnection::State::Disconnected:
      // connection disconnected, but should fix itself
      break;
    case rtc::PeerConnection::State::Connected:
      // \o/ we should use the connection
      break;
    case rtc::PeerConnection::State::Failed:
      // connection failed
    case rtc::PeerConnection::State::Closed:
      // connection closed we should clean up
      NPLOGI << "Webrtc Connection " << state;
      const std::lock_guard<std::mutex> lock(mutex_);
      pc = pc_;
      pc_ = nullptr;
      channel = channel_;
      channel_ = nullptr;
      break;
  }

  if (channel) {
    channel->close();
  }
};

void WebrtcConnection::handleSignalingStateChange(
    rtc::PeerConnection::SignalingState state) {
  NPLOGI << "Signaling state changed: " << state;
  if (state == rtc::PeerConnection::SignalingState::HaveLocalOffer) {
  } else if (state == rtc::PeerConnection::SignalingState::HaveRemoteOffer) {
    std::shared_ptr<rtc::PeerConnection> pc;
    {
      const std::lock_guard<std::mutex> lock(mutex_);
      pc = pc_;
    }
    pc->setLocalDescription();
  } else {
    std::ostringstream oss;
    oss << state;
    NPLOGD << "Got unhandled signaling state: " << oss.str();
    return;
  }
  const std::lock_guard<std::mutex> lock(mutex_);
  if (canTrickle_ ||
      pc_->gatheringState() == rtc::PeerConnection::GatheringState::Complete) {
    auto description = pc_->localDescription();
    sendDescription(description);
  }
}

void WebrtcConnection::handleLocalCandidate(rtc::Candidate cand) {
  const std::lock_guard<std::mutex> lock(mutex_);
  if (canTrickle_) {
    nabto::webrtc::util::SignalingCandidate candidate(cand.candidate());
    candidate.setSdpMid(cand.mid());

    sendSignalingMessage(
        nabto::webrtc::util::WebrtcSignalingMessage(candidate));
  }
}

void WebrtcConnection::handleTrackEvent(std::shared_ptr<rtc::Track> track) {}

void WebrtcConnection::handleDatachannelEvent(
    std::shared_ptr<rtc::DataChannel> channel) {
  auto self = shared_from_this();
  channel->onMessage(
      [self](rtc::binary data) { NPLOGE << "Got BINARY datachannel message "; },
      [self](std::string data) {
        NPLOGE << "Got datachannel message: " << data;
        // if (data == "Datachannel msg: 1") {
        //     self->trackRef_ = self->videoTrack_->addTrack(self->pc_);
        //     self->pc_->setLocalDescription();
        // }
      });

  channel->onOpen([self, channel]() {
    const std::lock_guard<std::mutex> lock(self->mutex_);
    NPLOGE << "Datachannel opened";
    std::stringstream ss;
    ss << "datachannel msg " << self->datachannelSendSeq_;
    std::string msg = ss.str();
    std::vector<std::byte> vec((const std::byte*)msg.data(),
                               (const std::byte*)msg.data() + msg.size());
    auto message =
        rtc::make_message(vec.begin(), vec.end(), rtc::Message::String);
    channel->send(rtc::to_variant(*message));
  });
}

void WebrtcConnection::addTrack() {
  if (videoTrack_ && pc_) {
    trackRef_ = videoTrack_->addTrack(pc_);
    pc_->setLocalDescription();
  }
}

void WebrtcConnection::handleGatherinsStateChange(
    rtc::PeerConnection::GatheringState state) {
  if (state == rtc::PeerConnection::GatheringState::Complete && !canTrickle_) {
    // TODO: Handle canTrickle false
    // auto description = pc_->localDescription();
    // nlohmann::json message = {
    //     {"type", description->typeString()},
    //     {"sdp", std::string(description.value())} };
    // auto data = message.dump();
    // if (description->type() == rtc::Description::Type::Offer) {
    //     NPLOGD << "Sending offer: " << std::string(description.value());
    //     sendDescription(description)
    //     // self->sigStream_->signalingSendOffer(data, self->metadata_);
    // }
    // else {
    //     NPLOGD << "Sending answer: " << std::string(description.value());
    //     // self->sigStream_->signalingSendAnswer(data, self->metadata_);
    // }
  }
}

void WebrtcConnection::sendDescription(
    rtc::optional<rtc::Description> description) {
  NPLOGD << "SendDescription with: "
         << (description.has_value() ? "true" : "false");
  if (description) {
    nabto::webrtc::util::SignalingDescription desc(
        description->typeString(), std::string(description.value()));
    sendSignalingMessage(nabto::webrtc::util::WebrtcSignalingMessage(desc));

    if (description->type() == rtc::Description::Type::Answer) {
      if (pc_->negotiationNeeded()) {
        // We where polite and have finished resolving a collided offer,
        // recreate our offer
        pc_->setLocalDescription();
      }
    }
  }
}

void WebrtcConnection::sendSignalingMessage(
    const nabto::webrtc::util::WebrtcSignalingMessage& message) {
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
      iceServers_.push_back(server);
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
    pc = pc_;
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
  pc_ = nullptr;
}

void WebrtcConnection::handleTransportError(
    const nabto::webrtc::SignalingError& error) {
  std::shared_ptr<rtc::PeerConnection> pc = nullptr;
  {
    const std::lock_guard<std::mutex> lock(mutex_);
    pc = pc_;
  }
  if (pc) {
    pc->close();
  } else {
    deinit();
  }
}

void WebrtcConnection::handleChannelError(
    const nabto::webrtc::SignalingError& error) {
  handleTransportError(error);
}

void WebrtcConnection::handleChannelStateChange(
    const nabto::webrtc::SignalingChannelState& state) {
  std::shared_ptr<rtc::PeerConnection> pc = nullptr;

  switch (state) {
    case nabto::webrtc::SignalingChannelState::OFFLINE:
      NPLOGD << "Got channel state: OFFLINE";
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
        pc = pc_;
      }
      if (pc) {
        pc->close();
      } else {
        deinit();
      }
      break;
    case nabto::webrtc::SignalingChannelState::ONLINE:
      NPLOGD << "Got channel state ONLINE";
      // This means client reconnected, the client should do ICE restart
      // if needed so we can just ignore.
      break;
    case nabto::webrtc::SignalingChannelState::FAILED:
      NPLOGD << "Got channel state: FAILED";
      {
        const std::lock_guard<std::mutex> lock(mutex_);
        pc = pc_;
      }
      if (pc) {
        pc->close();
      } else {
        deinit();
      }
      break;
  }
}

}  // namespace example
}  // namespace nabto
