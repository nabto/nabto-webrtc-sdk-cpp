#pragma once

#include <logging/logging.hpp>
#include <memory>
#include <rtc/rtc.hpp>
#include <webrtc_connection/track_handler.hpp>

#include "../utils/rtp-repacketizer/h264_repacketizer.hpp"
#include "rtsp-client/rtsp_client.hpp"

namespace nabto {
namespace example {

class H264TrackHandler;
typedef std::shared_ptr<H264TrackHandler> H264TrackHandlerPtr;

class SsrcGenerator {
 public:
  static uint32_t generateSsrc() {
    static std::mutex mutex;
    std::lock_guard<std::mutex> lock(mutex);

    static uint32_t ssrc = 0;

    ssrc += 1;
    return ssrc;
  }
};

class MidGenerator {
 public:
  static std::string generateMid() {
    static std::mutex mutex;
    std::lock_guard<std::mutex> lock(mutex);
    static uint64_t midCounter = 0;
    std::stringstream ss;
    ss << "device-" << midCounter;
    midCounter += 1;
    return ss.str();
  }
};

class HandlerTrack {
 public:
  std::shared_ptr<rtc::Track> videoTrack;
  std::shared_ptr<rtc::Track> audioTrack;
  RtspClientPtr rtp;
  size_t ref;
};

class H264TrackHandler : public WebrtcTrackHandler,
                         public std::enable_shared_from_this<H264TrackHandler> {
 public:
  static WebrtcTrackHandlerPtr create(std::string rtspUrl) {
    return std::make_shared<H264TrackHandler>(rtspUrl);
  }

  H264TrackHandler(std::string rtspUrl)
      : rtspUrl_(rtspUrl),
        ssrc_(SsrcGenerator::generateSsrc()),
        audioSsrc_(SsrcGenerator::generateSsrc()) {
    RtspClientConf conf = {rtspUrl, videoRepack_, nullptr,   96,
                           111,     ssrc_,        audioSsrc_};
    rtp_ = RtspClient::create(conf);
  }

  size_t addTrack(std::shared_ptr<rtc::PeerConnection> pc) {
    counter_++;
    RtspClientConf conf = {
        rtspUrl_,   videoRepack_, nullptr,
        96,         111,          ssrc_,
        audioSsrc_, true,         (uint16_t)(42222 + (counter_ * 4))};
    HandlerTrack track;

    track.rtp = RtspClient::create(conf);
    track.videoTrack = pc->addTrack(createVideoDescription());
    track.audioTrack = pc->addTrack(createAudioDescription());
    track.ref = track.rtp->addConnection(track.videoTrack, track.audioTrack);
    return track.ref;
  }

  void removeConnection(size_t ref) {
    auto conn = connections_.find(ref);
    if (conn == connections_.end()) {
      return;
    }
    conn->second.rtp->removeConnection(ref);
    connections_.erase(conn);
  }

  void close() {
    videoTrack_ = nullptr;
    rtp_ = nullptr;
  }

 private:
  RtpRepacketizerFactoryPtr videoRepack_ = H264RepacketizerFactory::create();
  std::shared_ptr<rtc::Track> videoTrack_;
  std::shared_ptr<rtc::Track> audioTrack_;
  RtspClientPtr rtp_;

  std::map<size_t, HandlerTrack> connections_;
  size_t counter_ = 0;

  std::string rtspUrl_;
  uint32_t ssrc_;
  uint32_t payloadType_ = 96;

  uint32_t audioSsrc_;

  rtc::Description::Video createVideoDescription() {
    // Create a Video media description.
    // We support both sending and receiving video
    std::string mid = MidGenerator::generateMid();
    rtc::Description::Video media(mid, rtc::Description::Direction::SendRecv);

    // Since we are creating the media track, only the supported payload type
    // exists, so we might as well reuse the same value for the RTP session in
    // WebRTC as the one we use in the RTP source (eg. Gstreamer)
    media.addH264Codec(payloadType_);

    // Libdatachannel H264 default codec is already using:
    // level-asymmetry-allowed=1
    // packetization-mode=1
    // profile-level-id=42e01f
    // However, again to be technically correct, we remove the unsupported
    // feedback extensions
    auto r = media.rtpMap(payloadType_);
    r->removeFeedback("nack");
    r->removeFeedback("goog-remb");
    return media;
  }

  rtc::Description::Audio createAudioDescription() {
    // Create an Audio media description.
    // We support both sending and receiving audio
    std::string mid = MidGenerator::generateMid();
    rtc::Description::Audio media(mid, rtc::Description::Direction::SendRecv);

    media.addOpusCodec(111);
    auto r = media.rtpMap(111);

    // media.addPCMUCodec(0);
    // auto r = media.rtpMap(0);

    r->removeFeedback("nack");
    r->removeFeedback("goog-remb");
    return media;
  }
};

}  // namespace example
}  // namespace nabto
