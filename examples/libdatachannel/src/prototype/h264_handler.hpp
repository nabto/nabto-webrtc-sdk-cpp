#pragma once

#include <memory>
#include <nabto/webrtc/util/logging.hpp>
#include <rtc/rtc.hpp>
#include <rtp_client/rtp_client.hpp>
#include "connection.hpp"

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
      : track_(track), ssrc_(SsrcGenerator::generateSsrc()) {
    if (track_) {
      handleIncomingTrack();
    } else {
      RtpClientConf conf = {"127.0.0.1", 6000};
      rtp_ = RtpClient::create(conf);
    }
  }

  size_t addTrack(std::shared_ptr<rtc::PeerConnection> pc) {
    track_ = pc->addTrack(createVideoDescription());
    return rtp_->addConnection(track_, ssrc_, payloadType_);
  }

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

  void removeConnection(size_t ref) { rtp_->removeConnection(ref); }

  void close() {
    track_ = nullptr;
    rtp_ = nullptr;
  }

 private:
  std::shared_ptr<rtc::Track> track_;
  RtpClientPtr rtp_;
  uint32_t ssrc_;
  uint32_t payloadType_ = 96;

  void handleIncomingTrack() {
    NPLOGD << "*** INCOMING TRACK ***";
  }
};

}  // namespace example
}  // namespace nabto
