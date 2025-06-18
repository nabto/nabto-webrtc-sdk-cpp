#pragma once

#include <nabto/webrtc/util/curl_async.hpp>

#include "../../utils/rtp-repacketizer/rtp_repacketizer.hpp"

namespace nabto {

class TcpRtpClient;
typedef std::shared_ptr<TcpRtpClient> TcpRtpClientPtr;

class TcpRtpClientConf {
 public:
  nabto::util::CurlAsyncPtr curl;
  std::string url;
  RtpRepacketizerFactoryPtr videoRepack;
  RtpRepacketizerFactoryPtr audioRepack;
  int videoPayloadType;
  int audioPayloadType;
  uint32_t videoSsrc;
  uint32_t audioSsrc;
};

class TcpRtpClient : public std::enable_shared_from_this<TcpRtpClient> {
 public:
  static TcpRtpClientPtr create(const TcpRtpClientConf& conf);
  TcpRtpClient(const TcpRtpClientConf& conf);

  ~TcpRtpClient();

  void setConnection(std::shared_ptr<rtc::Track> videoTrack,
                     std::shared_ptr<rtc::Track> audioTrack);

  void stop() {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      stopped_ = true;
    }
    curl_->stop();
  }

  void run();

 private:
  static size_t rtp_write(void* ptr, size_t size, size_t nmemb, void* userp);

  nabto::util::CurlAsyncPtr curl_;
  std::string url_;
  bool stopped_ = true;
  std::mutex mutex_;

  RtpRepacketizerFactoryPtr videoRepack_ = RtpRepacketizerFactory::create();
  RtpRepacketizerPtr videoRepacketizer_ = nullptr;
  std::shared_ptr<rtc::Track> videoTrack_ = nullptr;
  uint32_t videoSsrc_ = 0;
  int videoSrcPt_ = 0;
  int videoDstPt_ = 0;

  RtpRepacketizerFactoryPtr audioRepack_ = RtpRepacketizerFactory::create();
  RtpRepacketizerPtr audioRepacketizer_ = nullptr;
  std::shared_ptr<rtc::Track> audioTrack_ = nullptr;
  uint32_t audioSsrc_ = 0;
  int audioSrcPt_ = 0;
  int audioDstPt_ = 0;

  char rtcpWriteBuf_[64];
  bool sendRtcp_ = false;
};

}  // namespace nabto
