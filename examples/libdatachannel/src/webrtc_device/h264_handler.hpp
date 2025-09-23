#pragma once

#include <memory>
#include <cassert>
#include <nabto/webrtc/util/logging.hpp>
#include <rtc/rtc.hpp>
#include <rtp_client/rtp_client.hpp>
#include <webrtc_connection/track_handler.hpp>

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

class H264TrackHandler : public WebrtcTrackHandler,
                         public std::enable_shared_from_this<H264TrackHandler> {
 public:
  static H264TrackHandlerPtr create(std::shared_ptr<rtc::Track> track) {
    return std::make_shared<H264TrackHandler>(track);
  }

  H264TrackHandler(std::shared_ptr<rtc::Track> track)
      : videoSsrc_(SsrcGenerator::generateSsrc()),
        audioSsrc_(SsrcGenerator::generateSsrc()) {

    RtpClientConf videoConf = {"127.0.0.1", 6000};
    RtpClientConf audioConf = {"127.0.0.1", 6002};
    
    videoRtp_ = RtpClient::create(videoConf);
    audioRtp_ = RtpClient::create(audioConf);
  }

  size_t addTrack(std::shared_ptr<rtc::PeerConnection> pc) {
    // add AV tracks
    videoTrack_ = pc->addTrack(createVideoDescription());
    audioTrack_ = pc->addTrack(createAudioDescription());

    size_t videoRef = videoRtp_->addConnection(videoTrack_, videoSsrc_, videoPt_);
    size_t audioRef = audioRtp_->addConnection(audioTrack_, audioSsrc_, audioPt_);

    assert(videoRef == audioRef);

    return videoRef;
  }

  rtc::Description::Video createVideoDescription() {
    // Create a Video media description.
    // We support both sending and receiving video
    std::string mid = MidGenerator::generateMid();
    rtc::Description::Video media(mid, rtc::Description::Direction::SendRecv);

    // Since we are creating the media track, only the supported payload type
    // exists, so we might as well reuse the same value for the RTP session in
    // WebRTC as the one we use in the RTP source (eg. Gstreamer)
    media.addH264Codec(videoPt_);

    // Libdatachannel H264 default codec is already using:
    // level-asymmetry-allowed=1
    // packetization-mode=1
    // profile-level-id=42e01f
    // However, again to be technically correct, we remove the unsupported
    // feedback extensions
    auto r = media.rtpMap(videoPt_);
    r->removeFeedback("nack");
    r->removeFeedback("goog-remb");
    return media;
  }

  rtc::Description::Audio createAudioDescription() {
    // Create an Audio media description.
    // We support both sending and receiving audio
    std::string mid = MidGenerator::generateMid();
    rtc::Description::Audio media(mid, rtc::Description::Direction::SendRecv);

    media.addOpusCodec(audioPt_);
    auto r = media.rtpMap(audioPt_);

    // media.addPCMUCodec(0);
    // auto r = media.rtpMap(0);
    r->removeFeedback("nack");
    r->removeFeedback("goog-remb");
    return media;
  }

  void removeConnection(size_t ref) {
    videoRtp_->removeConnection(ref);
    audioRtp_->removeConnection(ref);
  }

  void close() {
    videoTrack_ = nullptr;
    audioTrack_ = nullptr;
    videoRtp_ = nullptr;
    audioRtp_ = nullptr;
  }

 private:
  std::shared_ptr<rtc::Track> videoTrack_;
  std::shared_ptr<rtc::Track> audioTrack_;
  RtpClientPtr videoRtp_;
  RtpClientPtr audioRtp_;
  uint32_t videoSsrc_;
  uint32_t audioSsrc_;
  uint32_t videoPt_ = 96;
  uint32_t audioPt_ = 111;
};

}  // namespace example
}  // namespace nabto
