#include "tcp_rtp_client.hpp"

#include <curl/curl.h>

#include <nabto/webrtc/util/logging.hpp>
#include <rtc/rtc.hpp>

namespace nabto {

TcpRtpClientPtr TcpRtpClient::create(const TcpRtpClientConf& conf) {
  // THIS IS CALLED FROM THE CURL WORKER THREAD!
  return std::make_shared<TcpRtpClient>(conf);
}

TcpRtpClient::TcpRtpClient(const TcpRtpClientConf& conf) {
  NPLOGD << "TcpRtpClient constructor";
  curl_ = conf.curl;
  url_ = conf.url;
  if (conf.videoRepack != nullptr) {
    videoRepack_ = conf.videoRepack;
  }
  if (conf.audioRepack != nullptr) {
    audioRepack_ = conf.audioRepack;
  }

  videoSrcPt_ = conf.videoPayloadType;
  videoSsrc_ = conf.videoSsrc;
  audioSrcPt_ = conf.audioPayloadType;
  audioSsrc_ = conf.audioSsrc;
}

TcpRtpClient::~TcpRtpClient() {}

void TcpRtpClient::setConnection(std::shared_ptr<rtc::Track> videoTrack,
                                 std::shared_ptr<rtc::Track> audioTrack) {
  NPLOGD << "TcpRtpClient setConnection";
  if (videoTrack != nullptr) {
    std::lock_guard<std::mutex> lock(mutex_);
    videoTrack_ = std::move(videoTrack);
    rtc::Description::Media desc = videoTrack_->description();
    auto pts = desc.payloadTypes();
    int pt = pts.empty() ? 0 : pts[0];
    videoRepacketizer_ =
        videoRepack_->createPacketizer(videoTrack_, videoSsrc_, pt);
    videoDstPt_ = pt;
  }
  if (audioTrack != nullptr) {
    std::lock_guard<std::mutex> lock(mutex_);
    audioTrack_ = std::move(audioTrack);
    rtc::Description::Media desc = audioTrack_->description();
    auto pts = desc.payloadTypes();
    int pt = pts.empty() ? 0 : pts[0];
    audioRepacketizer_ =
        audioRepack_->createPacketizer(audioTrack_, audioSsrc_, pt);
    audioDstPt_ = pt;
  }
}

void TcpRtpClient::run() {
  NPLOGD << "TcpRtpClient run";
  {
    std::lock_guard<std::mutex> lock(mutex_);
    stopped_ = false;
  }

  auto curl = curl_->getCurl();
  CURLcode res = CURLE_OK;
  while (1) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (stopped_) {
        NPLOGD << "TcpRtpClient got stopped";
        break;
      }
    }
    if ((res = curl_easy_setopt(curl, CURLOPT_INTERLEAVEFUNCTION,
                                &TcpRtpClient::rtp_write)) != CURLE_OK ||
        (res = curl_easy_setopt(curl, CURLOPT_INTERLEAVEDATA, this)) !=
            CURLE_OK ||
        (res = curl_easy_setopt(curl, CURLOPT_RTSP_REQUEST,
                                (long)CURL_RTSPREQ_RECEIVE)) != CURLE_OK) {
      NPLOGE << "Failed to create RECEIVE request with: "
             << curl_easy_strerror(res);
      break;
    }

    uint16_t status = 0;
    curl_->reinvokeStatus(&res, &status);

    if (res != CURLE_OK) {
      NPLOGE << "Failed to perform RTSP RECEIVE request with: "
             << curl_easy_strerror(res);
      break;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (sendRtcp_) {
      sendRtcp_ = false;
      CURLcode res = CURLE_OK;
      curl_socket_t sockfd;
      res = curl_easy_getinfo(curl, CURLINFO_ACTIVESOCKET, &sockfd);
      if (!res && sockfd != CURL_SOCKET_BAD) {
        uint16_t len = *(uint16_t*)(rtcpWriteBuf_ + 2) + 4;
        // NPLOGD << "Sending RR on channel: " << (int)rtcpWriteBuf_[1];
        auto ret = write(sockfd, rtcpWriteBuf_, len);
        if (ret < len) {
          NPLOGE << "Failed to write RTCP RR to TCP socket. ret: " << ret;
          break;
        }
      } else {
        NPLOGE << "Failed to get active socket: " << curl_easy_strerror(res)
               << " Sock: " << sockfd;
      }
    }
  }
  NPLOGD << "TcpRtpClient run returning";
}

size_t TcpRtpClient::rtp_write(void* ptr, size_t size, size_t nmemb,
                               void* userp) {
  TcpRtpClient* self = (TcpRtpClient*)userp;
  size_t len = size * nmemb;
  if (len < 4) {
    NPLOGE << "Received data len < 4 which should not be possible";
    return len;
  }

  if (((char*)ptr)[0] != '$') {
    NPLOGE << "Received data did not start with $!";
    return len;
  }

  uint16_t dataLen = ((uint8_t*)ptr)[3] + (((uint8_t*)ptr)[2] << 8);
  uint8_t channel = ((uint8_t*)ptr)[1];
  // NPLOGE << "Received data on channel: " << (int)channel << " with len: " <<
  // len << " dataLen: " << dataLen;

  if (channel == 0) {
    // video RTP
    std::lock_guard<std::mutex> lock(self->mutex_);
    if (self->videoTrack_ != nullptr && self->videoTrack_->isOpen()) {
      uint8_t* buf = ((uint8_t*)ptr) + 4;
      auto packets = self->videoRepacketizer_->handlePacket(
          std::vector<uint8_t>(buf, buf + dataLen));
      try {
        for (auto p : packets) {
          self->videoTrack_->send((rtc::byte*)p.data(), p.size());
        }
      } catch (std::runtime_error err) {
        // This was introduced as we observed a runtime error due to the track
        // being closed. Since we check for isOpen(), this appears to be a race
        // condition. Both us and libdatachannel have mutex protection, so this
        // will not be a memory issue. However, libdatachannel can still change
        // the Open state between the check above and this.
        NPLOGE << "Caught send runtime error: " << err.what();
      }
    }
  } else if (channel == 2) {
    // Audio RTP
    std::lock_guard<std::mutex> lock(self->mutex_);
    if (self->audioTrack_ != nullptr && self->audioTrack_->isOpen()) {
      try {
        uint8_t* buf = ((uint8_t*)ptr) + 4;
        auto packets = self->audioRepacketizer_->handlePacket(
            std::vector<uint8_t>(buf, buf + dataLen));
        for (auto p : packets) {
          self->audioTrack_->send((rtc::byte*)p.data(), p.size());
        }
      } catch (std::runtime_error err) {
        // This was introduced as we observed a runtime error due to the track
        // being closed. Since we check for isOpen(), this appears to be a race
        // condition. Both us and libdatachannel have mutex protection, so this
        // will not be a memory issue. However, libdatachannel can still change
        // the Open state between the check above and this.
        NPLOGE << "Caught send runtime error: " << err.what();
      }
    }
  } else {
    std::lock_guard<std::mutex> lock(self->mutex_);
    char* buffer = (char*)ptr + 4;
    auto sr = reinterpret_cast<rtc::RtcpSr*>(buffer);
    memset(self->rtcpWriteBuf_, 0, 64);
    self->rtcpWriteBuf_[0] = '$';
    self->rtcpWriteBuf_[1] = channel;
    rtc::RtcpRr* rr = (rtc::RtcpRr*)(self->rtcpWriteBuf_ + 4);
    rtc::RtcpReportBlock* rb = rr->getReportBlock(0);
    rb->preparePacket(sr->senderSSRC(), 0, 0, 0, 0, 0, sr->ntpTimestamp(), 0);
    rr->preparePacket(1, 1);
    uint16_t* p = (uint16_t*)&self->rtcpWriteBuf_[2];
    *p = rr->header.lengthInBytes();
    self->sendRtcp_ = true;
  }

  return len;
}

}  // namespace nabto
