#pragma once

#include <memory>
#include <nabto/webrtc/util/logging.hpp>
#include <rtc/rtc.hpp>
#include <rtp_client/rtp_client.hpp>
#include <webrtc_connection/track_handler.hpp>

#include "codecs.hpp"

namespace nabto::example {

class RtpTrackHandler;
typedef std::shared_ptr<RtpTrackHandler> RtpTrackHandlerPtr;

static uint32_t generateSsrc() {
  static std::mutex mutex;
  std::lock_guard<std::mutex> lock(mutex);

  static uint32_t ssrc = 0;

  ssrc += 1;
  return ssrc;
}

static std::string generateMid() {
  static std::mutex mutex;
  std::lock_guard<std::mutex> lock(mutex);
  static uint64_t midCounter = 0;
  std::stringstream ss;
  ss << "device-" << midCounter;
  midCounter += 1;
  return ss.str();
}

class RtpTrackHandler : public WebrtcTrackHandler,
                         public std::enable_shared_from_this<RtpTrackHandler> {
 public:
  static RtpTrackHandlerPtr create(VideoCodec videoCodec, AudioCodec audioCodec) {
    return std::make_shared<RtpTrackHandler>(videoCodec, audioCodec);
  }

  RtpTrackHandler(VideoCodec videoCodec, AudioCodec audioCodec)
      : videoSsrc_(generateSsrc()),
        audioSsrc_(generateSsrc()),
        videoCodec_(videoCodec),
        audioCodec_(audioCodec) {
    
    if (videoCodec_ != VideoCodec::NONE) {
      RtpClientConf videoConf = {"127.0.0.1", 6000};
      videoRtp_ = RtpClient::create(videoConf);
    }

    if (audioCodec_ != AudioCodec::NONE) {
      RtpClientConf audioConf = {"127.0.0.1", 6002};
      audioRtp_ = RtpClient::create(audioConf);
    }
  }

  size_t addTrack(std::shared_ptr<rtc::PeerConnection> pc) {
    size_t ref = 0;

    if (videoCodec_ != VideoCodec::NONE) {
      videoTrack_ = pc->addTrack(createVideoDescription());
      ref = videoRtp_->addConnection(videoTrack_, videoSsrc_, videoPt_);
    }

    if (audioCodec_ != AudioCodec::NONE) {
      audioTrack_ = pc->addTrack(createAudioDescription());
      ref = audioRtp_->addConnection(audioTrack_, audioSsrc_, audioPt_);
    }

    return ref;
  }

  rtc::Description::Video createVideoDescription() {
    std::string mid = generateMid();
    rtc::Description::Video media(mid, rtc::Description::Direction::SendRecv);

    switch (videoCodec_) {
      case VideoCodec::NONE: break;
      case VideoCodec::H264: media.addH264Codec(videoPt_); break;
      case VideoCodec::H265: media.addH265Codec(videoPt_); break;
      case VideoCodec::VP8: media.addVP8Codec(videoPt_); break;
    }

    auto r = media.rtpMap(videoPt_);
    r->removeFeedback("nack");
    r->removeFeedback("goog-remb");
    return media;
  }

  rtc::Description::Audio createAudioDescription() {
    std::string mid = generateMid();
    rtc::Description::Audio media(mid, rtc::Description::Direction::SendRecv);

    switch (audioCodec_) {
      case AudioCodec::NONE: break;
      case AudioCodec::OPUS: media.addOpusCodec(audioPt_);
      case AudioCodec::PCMU: media.addPCMUCodec(audioPt_);
    }

    auto r = media.rtpMap(audioPt_);
    r->removeFeedback("nack");
    r->removeFeedback("goog-remb");
    return media;
  }

  void removeConnection(size_t ref) {
    if (videoRtp_) { videoRtp_->removeConnection(ref); }
    if (audioRtp_) { audioRtp_->removeConnection(ref); }
  }

  void close() {
    videoTrack_ = nullptr;
    audioTrack_ = nullptr;
    videoRtp_ = nullptr;
    audioRtp_ = nullptr;
  }

 private:
  VideoCodec videoCodec_;
  AudioCodec audioCodec_;

  std::shared_ptr<rtc::Track> videoTrack_;
  std::shared_ptr<rtc::Track> audioTrack_;
  RtpClientPtr videoRtp_;
  RtpClientPtr audioRtp_;
  uint32_t videoSsrc_;
  uint32_t audioSsrc_;
  uint32_t videoPt_ = 96;
  uint32_t audioPt_ = 111;
};

}  // namespace nabto::example
