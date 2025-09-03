#pragma once

#include <memory>
#include <nabto/webrtc/util/logging.hpp>
#include <rtc/rtc.hpp>
#include <webrtc_connection/track_handler.hpp>

#include "rtsp-client/rtsp_client.hpp"

namespace nabto {
namespace example {

class H265TrackHandler;
typedef std::shared_ptr<H265TrackHandler> H265TrackHandlerPtr;

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

class H265TrackHandler : public WebrtcTrackHandler,
                         public std::enable_shared_from_this<H265TrackHandler> {
 public:
  static WebrtcTrackHandlerPtr create(std::string rtspUrl) {
    return std::make_shared<H265TrackHandler>(rtspUrl);
  }

  H265TrackHandler(std::string rtspUrl)
      : rtspUrl_(rtspUrl),
        ssrc_(SsrcGenerator::generateSsrc()),
        audioSsrc_(SsrcGenerator::generateSsrc()) {
    RtspClientConf conf = {rtspUrl, nullptr, nullptr,   96,
                           111,     ssrc_,   audioSsrc_};
    rtp_ = RtspClient::create(conf);
  }

  size_t addTrack(std::shared_ptr<rtc::PeerConnection> pc) {
    counter_++;
    RtspClientConf conf = {
        rtspUrl_,   nullptr, nullptr,
        96,         111,     ssrc_,
        audioSsrc_, true,    (uint16_t)(42222 + (counter_ * 4))};
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

    // media.addVideoCodec(
    //     96,
    //     "m=video 0 RTP/AVP 96\r\nc=IN IP4 0.0.0.0\r\na=rtpmap:96 "
    //     "H265/90000\r\na=framerate:30\r\na=fmtp:96 "
    //     "sprop-vps=QAEMAf//"
    //     "BAgAAAMAmAgAAAMAAFqSgJA=;sprop-sps="
    //     "QgEBBAgAAAMAmAgAAAMAAFqQAKBAPCKUslSSZX/"
    //     "4AAgAC1BgYGBAAAADAEAAAAeC;sprop-pps=RAHBcoYMRiQ=\r\na=control:"
    //     "stream=0\r\na=ts-refclk:local\r\na=mediaclk:sender\r\na=ssrc:"
    //     "1842951636 cname:user1268947453@host-65e41dfd\r\n");

    // Since we are creating the media track, only the supported payload
    // type exists, so we might as well reuse the same value for the RTP
    // session in WebRTC as the one we use in the RTP source (eg. Gstreamer)
    media.addH265Codec(payloadType_);
    // auto r = media.rtpMap(96);
    // r->fmtps.push_back(
    //     "sprop-vps=QAEMAf//"
    //     "AWAAAAMAkAAAAwAAAwB4koCQ;sprop-sps="
    //     "QgEBAWAAAAMAkAAAAwAAAwB4oAPAgBDllkqSTK//"
    //     "AAEAAWoCAgIIAAADAAgAAAMA8EA=;sprop-pps=RAHBcrRiQA==");

    // media.addSSRC(ssrc_, mid, mid);

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
