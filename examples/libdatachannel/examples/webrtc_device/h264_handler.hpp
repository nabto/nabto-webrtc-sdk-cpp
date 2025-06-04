#pragma once

#include <memory>
#include <nabto/webrtc/util/logging.hpp>
#include <rtc/rtc.hpp>
#include <webrtc_connection/track_handler.hpp>

#include "../utils/rtp-client/rtp_client.hpp"

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
    RtpClientConf conf = {"127.0.0.1", 6000};
    rtp_ = RtpClient::create(conf);
    if (track_) {
      handleIncomingTrack();
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
    // TODO: support incoming tracks
    /*
    // We must go through all codecs in the Media Description and remove all
    codecs other than the one we support.

    auto media = track_->description();

    rtc::Description::Media::RtpMap* rtp = NULL;
    bool found = false;
    // Loop all payload types offered by the client
    for (auto pt : media.payloadTypes()) {
        rtc::Description::Media::RtpMap* r = NULL;
        try {
            // Get the RTP description for this payload type
            r = media.rtpMap(pt);
        } catch (std::exception& ex) {
            // Since we are getting the description based on the list of payload
    types this should never fail, but just in case. NPLOGE << "Bad rtpMap for
    pt: " << pt; continue;
        }
        // If this payload type is H264/90000
        if (r->format == "H264" && r->clockRate == 90000) {
            // We also want the codec to match:
            // level-asymmetry-allowed=1
            // packetization-mode=1
            // profile-level-id=42e01f
            std::string profLvlId = "42e01f";
            std::string lvlAsymAllowed = "1";
            std::string pktMode = "1";

            // However, through trial and error, these does not always have to
    match perfectly, so for a bit of flexibility we allow any H264 codec.
            // If a later codec matches perfectly we update our choice.
            if (found && r->fmtps.size() > 0 &&
                r->fmtps[0].find("profile-level-id=" + profLvlId) !=
    std::string::npos && r->fmtps[0].find("level-asymmetry-allowed=" +
    lvlAsymAllowed) != std::string::npos &&
                r->fmtps[0].find("packetization-mode=" + pktMode) !=
    std::string::npos ) {
                // Found better match use this
                media.removeRtpMap(rtp->payloadType);
                rtp = r;
                NPLOGD << "FOUND RTP BETTER codec match!!! " << pt;
            }
            else if (found) {
                NPLOGD << "h264 pt: " << pt << " no match, removing";
                media.removeRtpMap(pt);
                continue;
            }
            else {
                NPLOGD << "FOUND RTP codec match!!! " << pt;
            }
            found = true; // found a match, just remove any remaining rtpMaps
            rtp = r;
            NPLOGD << "Format: " << rtp->format << " clockRate: " <<
    rtp->clockRate << " encParams: " << rtp->encParams; NPLOGD << "rtcp fbs:";
            for (auto s : rtp->rtcpFbs) {
                NPLOGD << "   " << s;
            }

            // Our implementation does not support these feedback extensions, so
    we remove them (if they exist)
            // Though the technically correct way to do it, trial and error has
    shown this has no practial effect. rtp->removeFeedback("nack");
            rtp->removeFeedback("goog-remb");
            rtp->removeFeedback("transport-cc");
            rtp->removeFeedback("ccm fir");
        }
        else {
            // We remove any payload type not matching our codec
            NPLOGD << "pt: " << pt << " no match, removing";
            media.removeRtpMap(pt);
        }
    }
    if (rtp == NULL) {
        return;
    }
    // Add the ssrc to the track
    media.addSSRC(ssrc_, "videofeed");
    track_->setDescription(media);
    return;
    */
  }
};

}  // namespace example
}  // namespace nabto
