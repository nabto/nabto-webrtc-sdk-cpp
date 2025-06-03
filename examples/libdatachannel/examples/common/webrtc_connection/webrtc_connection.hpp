#pragma once
#include <nabto/signaling/signaling.hpp>
#include <message_signer/message_signer.hpp>
#include "track_handler.hpp"

#include <rtc/rtc.hpp>

#include <memory>

namespace nabto { namespace example {
class WebrtcConnection;
typedef std::shared_ptr<WebrtcConnection> WebrtcConnectionPtr;

class WebrtcConnection : public std::enable_shared_from_this<WebrtcConnection> {
public:
    static WebrtcConnectionPtr create(nabto::signaling::SignalingDevicePtr device, nabto::signaling::SignalingChannelPtr sig, MessageSignerPtr messageSigner, WebrtcTrackHandlerPtr trackHandler);
    WebrtcConnection(nabto::signaling::SignalingDevicePtr device, nabto::signaling::SignalingChannelPtr channel, MessageSignerPtr messageSigner, WebrtcTrackHandlerPtr trackHandler);

    void handleMessage(const nlohmann::json& msg);

    // TODO: add this
    // void handleReconnect();

private:
    nabto::signaling::SignalingChannelPtr channel_;
    nabto::signaling::SignalingDevicePtr device_;
    MessageSignerPtr messageSigner_;
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
    void sendCreateResponse(const std::vector<struct nabto::signaling::IceServer>& iceServers);
    void requestIceServers();
    void parseIceServers(std::vector<struct nabto::signaling::IceServer> &servers);

    void sendSignalingMessage(const nlohmann::json& message);

    void addTrack();

    size_t datachannelSendSeq_ = 0;
};

} // namespace
}
