#include "webrtc_connection.hpp"
#include <logging/logging.hpp>
#include <nlohmann/json.hpp>

namespace nabto { namespace example {

WebrtcConnectionPtr WebrtcConnection::create(nabto::signaling::SignalingChannelPtr sig, MessageSignerPtr messageSigner, WebrtcTrackHandlerPtr trackHandler) {
    auto p = std::make_shared<WebrtcConnection>(sig, messageSigner, trackHandler);
    p->init();
    return p;
}

WebrtcConnection::WebrtcConnection(nabto::signaling::SignalingChannelPtr sig, MessageSignerPtr messageSigner, WebrtcTrackHandlerPtr trackHandler) : sig_(sig), messageSigner_(messageSigner), videoTrack_(trackHandler), polite_(sig->isDevice())
{
}

void WebrtcConnection::init()
{
    // createPeerConnection();
    auto self = shared_from_this();
    requestIceServers();
    sig_->setMessageHandler([self](const std::string& msg) {
        self->handleMessage(msg);
    });
    sig_->setEventHandler([self](nabto::signaling::SignalingChannelEventType event) {

        switch(event) {
            case nabto::signaling::SignalingChannelEventType::CLIENT_OFFLINE:
                NPLOGD << "Got CLIENT_OFFLINE";
                // This means we tried to send a signaling message but the client is not connected to the backend.
                // If we expect the client to connect momentarily, we will then get a CLIENT_CONNECTED event and the reliability layer will fix it and we can ignore this event.
                // Otherwise we should handle the error.
                break;
            case nabto::signaling::SignalingChannelEventType::SIGNALING_CLOSED:
                NPLOGD << "Got SIGNALING_CLOSED";
                self->pc_->close();
                break;
            case nabto::signaling::SignalingChannelEventType::CLIENT_CONNECTED:
                NPLOGD << "Got CLIENT_CONNECTED";
                // TODO: This means client reconnected, we should probably checkAlive(), but really the client should do ICE restart if needed and we can just ignore. Also, if we had unacked messages the reliability layer will have resent them.
                break;
            case nabto::signaling::SignalingChannelEventType::SIGNALING_RECONNECT:
                NPLOGD << "Got SIGNALING_RECONNECT";
                // TODO: our WS connection reconnected, our webrtc connection may also be affected by the cause of this reconnect, so we may have to call iceRestart();
                break;
        }
    });

    sig_->setErrorHandler([self](const nabto::signaling::SignalingError& error) {
        NPLOGE << "Got errorCode: " << error.errorCode();
        // All errors are fatal, so we clean up no matter what the error was
        if (self->pc_) {
            self->pc_->close();
        } else {
            self->deinit();
        }
    });
}
void WebrtcConnection::handleMessage(const std::string& msgStr)
{
    if (!hasIce_) {
        messageQueue_.push_back(msgStr);
        return;
    }
    try {
        auto verifiedMessage = messageSigner_->verifyMessage(msgStr);

        nlohmann::json msg = nlohmann::json::parse(verifiedMessage);
        NPLOGI << "Webrtc got signaling message: " << verifiedMessage;
        auto type = msg["type"].get<std::string>();
        if (type == "DESCRIPTION") {
            rtc::Description remDesc(msg["description"]["sdp"].get<std::string>(), msg["description"]["type"].get<std::string>());

            bool offerCollision =
                remDesc.type() == rtc::Description::Type::Offer &&
                pc_->signalingState() != rtc::PeerConnection::SignalingState::Stable;

            ignoreOffer_ = !polite_ && offerCollision;
            if (ignoreOffer_) {
                NPLOGD << "The device is impolite and there is a collision so we are discarding the received offer";
                return;
            }

            pc_->setRemoteDescription(remDesc);
        } else if (type == "CANDIDATE") {
            try {
                rtc::Candidate cand(msg["candidate"]["candidate"].get<std::string>(), msg["candidate"]["sdpMid"].get<std::string>());
                pc_->addRemoteCandidate(cand);
            } catch (nlohmann::json::exception& ex) {
                NPLOGE << "handleIce json exception: " << ex.what();
            } catch (std::logic_error& ex) {
                if (!this->ignoreOffer_) {
                    NPLOGE << "Failed to add remote ICE candidate with logic error: " << ex.what();
                }
            }
        }
        else if (type == "CREATE_REQUEST") {
            // Ignore since init has triggered the response

        } else {
            NPLOGE << "Got unknown message type: " << type;
        }
    } catch (nabto::example::VerificationError& ex) {
        NPLOGE << "Could not verify the incoming signaling message: " << msgStr << " with: " << ex.what();
        sig_->sendError(nabto::signaling::SignalingError("VERIFICATION_ERROR", "Could not verify the incoming signaling message"));
    } catch (std::exception& ex) {
        NPLOGE << "Failed to handle message: " << msgStr << " with: " << ex.what();
    }
}

void WebrtcConnection::createPeerConnection()
{
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

void WebrtcConnection::handleStateChange(const rtc::PeerConnection::State& state)
{

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
        // connection fail and we should fix it
    case rtc::PeerConnection::State::Closed:
        // connection closed we should clean up
        NPLOGI << "Webrtc Connection " << state;
        // sig_->close();
        deinit();
        break;
    }
};

void WebrtcConnection::handleSignalingStateChange(rtc::PeerConnection::SignalingState state)
{
    NPLOGI << "Signaling state changed: " << state;
    if (state ==
        rtc::PeerConnection::SignalingState::HaveLocalOffer) {

        } else if (
        state == rtc::PeerConnection::SignalingState::HaveRemoteOffer) {
            pc_->setLocalDescription();
    }
    else {
        std::ostringstream oss;
        oss << state;
        NPLOGD << "Got unhandled signaling state: " << oss.str();
        return;
    }
    if (canTrickle_ || pc_->gatheringState() == rtc::PeerConnection::GatheringState::Complete) {
        auto description = pc_->localDescription();
        sendDescription(description);
    }

}

void WebrtcConnection::handleLocalCandidate(rtc::Candidate cand)
{
    if (canTrickle_) {
        nlohmann::json candidate = {
            {"sdpMid", cand.mid()},
            {"candidate", cand.candidate()}
        };

        nlohmann::json msg = {
            {"type", "CANDIDATE"},
            {"candidate", candidate},
        };

        auto msgStr = msg.dump();
        sendSignalingMessage(msgStr);
    }
}


void WebrtcConnection::handleTrackEvent(std::shared_ptr<rtc::Track> track)
{
}

void WebrtcConnection::handleDatachannelEvent(std::shared_ptr<rtc::DataChannel> channel)
{
    auto self = shared_from_this();
    channel->onMessage([self](rtc::binary data) {
        NPLOGE << "Got BINARY datachannel message ";
        },
        [self](std::string data) {
            NPLOGE << "Got datachannel message: " << data;
            // if (data == "Datachannel msg: 1") {
            //     self->trackRef_ = self->videoTrack_->addTrack(self->pc_);
            //     self->pc_->setLocalDescription();
            // }

        });

    channel->onOpen([self, channel]() {
        NPLOGE << "Datachannel opened";
        std::stringstream ss;
        ss << "datachannel msg " << self->datachannelSendSeq_;
        std::string msg = ss.str();
        std::vector<std::byte> vec((const std::byte*)msg.data(), (const std::byte*)msg.data() + msg.size());
        auto message = rtc::make_message(vec.begin(), vec.end(), rtc::Message::String);
        channel->send(rtc::to_variant(*message));
    });
}

void WebrtcConnection::addTrack()
{
    if (videoTrack_ && pc_) {
        trackRef_ = videoTrack_->addTrack(pc_);
        pc_->setLocalDescription();
    }
}


void WebrtcConnection::handleGatherinsStateChange(rtc::PeerConnection::GatheringState state)
{
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

void WebrtcConnection::sendCreateResponse(const std::vector<struct nabto::signaling::IceServer>& iceServers)
{
    nlohmann::json root;
    root["type"] = "CREATE_RESPONSE";
    root["iceServers"] = nlohmann::json::array();
    for (auto iceServer : iceServers) {
        nlohmann::json is;
        nlohmann::json urls = nlohmann::json::array();

        for (auto url : iceServer.urls) {
            urls.push_back(nlohmann::json::string_t(url));
        }

        is["urls"] = urls;
        if (!iceServer.credential.empty()) {
            is["credential"] = iceServer.credential;
        }
        if (!iceServer.username.empty()) {
            is["username"] = iceServer.username;
        }
        root["iceServers"].push_back(is);
    }
    auto msgStr = root.dump();
    sendSignalingMessage(msgStr);
}

void WebrtcConnection::sendDescription(rtc::optional<rtc::Description> description)
{
    NPLOGD << "SendDescription with: " << (description.has_value() ? "true" : "false");
    if (description)
    {
        nlohmann::json message = {
            {"type", description->typeString()},
            {"sdp", std::string(description.value())} };

        nlohmann::json msg = {
            {"type", "DESCRIPTION"},
            {"description", message},
        };

        auto msgStr = msg.dump();

        sendSignalingMessage(msgStr);

        if (description->type() == rtc::Description::Type::Answer)
        {
            if (pc_->negotiationNeeded()) {
                // We where polite and have finished resolving a collided offer, recreate our offer
                pc_->setLocalDescription();
            }
        }
    }

}

void WebrtcConnection::sendSignalingMessage(const std::string& message)
{
    try {
        auto signedMessage = messageSigner_->signMessage(message);
        sig_->sendMessage(signedMessage);
    } catch (std::exception& e) {
        NPLOGE << "Failed to sign the message: " << message << ", error: " << e.what();
    }
}

void WebrtcConnection::requestIceServers()
{
    auto self = shared_from_this();
    sig_->requestIceServers([self](std::vector<struct nabto::signaling::IceServer> servers) {
        self->parseIceServers(servers);
    });
}

void WebrtcConnection::parseIceServers(std::vector<struct nabto::signaling::IceServer>& servers)
{
    for (auto s : servers) {
        std::string proto = "";
        if (s.username.empty()) {
            proto = "stun:";
        }
        else {
            proto = "turn:";
        }
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
                << "    type: " << (server.type == rtc::IceServer::Type::Turn ? "TURN" : "STUN") << std::endl
                << "    RelayType: " << (server.relayType == rtc::IceServer::RelayType::TurnUdp ? "TurnUdp" : (server.relayType == rtc::IceServer::RelayType::TurnTcp ? "TurnTcp" : "TurnTls"));
            iceServers_.push_back(server);
        }
    }
    sendCreateResponse(servers);
    createPeerConnection();
    addTrack();
    hasIce_ = true;
    for (auto m : messageQueue_) {
        handleMessage(m);
    }
    messageQueue_.clear();
}

void WebrtcConnection::deinit()
{
    if (trackRef_ != 0 && videoTrack_ != nullptr) {
        videoTrack_->removeConnection(trackRef_);
    }
    videoTrack_ = nullptr;
    sig_->close();
    //sig_ = nullptr;
    // pc_->close();
    pc_ = nullptr;
}

} // namespace
}
