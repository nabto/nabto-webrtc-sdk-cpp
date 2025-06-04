#include "rtp_client.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>

#include <iomanip>  // For std::setfill and std::setw
#include <nabto/webrtc/util/logging.hpp>

const int RTP_BUFFER_SIZE = 2048;

namespace nabto {
namespace example {

RtpClientPtr RtpClient::create(const RtpClientConf& conf) {
  return std::make_shared<RtpClient>(conf);
}

RtpClient::RtpClient(const RtpClientConf& conf)
    : remoteHost_(conf.remoteHost),
      videoPort_(conf.port),
      remotePort_(conf.port + 1) {}

RtpClient::~RtpClient() {}

size_t RtpClient::addConnection(std::shared_ptr<rtc::Track> track,
                                const rtc::SSRC ssrc, int payloadType) {
  rtc::Description::Media desc = track->description();
  auto pts = desc.payloadTypes();

  // exactly 1 payload type should exist, else something has failed previously,
  // so we just pick the first one blindly.
  int pt = pts.empty() ? 0 : pts[0];

  index_++;
  RtpTrack t = {index_, track, ssrc, payloadType, pt};
  addConnection(t);
  return index_;
}

void RtpClient::addConnection(RtpTrack track) {
  std::lock_guard<std::mutex> lock(mutex_);
  mediaTracks_.push_back(track);
  NPLOGD << "Adding RTP connection pt " << track.srcPayloadType << "->"
         << track.dstPayloadType;
  if (stopped_) {
    start();
  }

  // We are also gonna receive data
  NPLOGD << "    adding Track receiver";
  auto self = shared_from_this();
  track.track->onMessage([self, track](rtc::message_variant data) {
    auto msg = rtc::make_message(data);

    if (msg->type == rtc::Message::Binary) {
      rtc::byte* data = msg->data();
      auto rtp = reinterpret_cast<rtc::RtpHeader*>(msg->data());

      uint8_t pt = rtp->payloadType();
      if (pt != track.dstPayloadType) {
        return;
      }

      rtp->setPayloadType(track.srcPayloadType);

      struct sockaddr_in addr = {};
      addr.sin_family = AF_INET;
      addr.sin_addr.s_addr = inet_addr(self->remoteHost_.c_str());
      addr.sin_port = htons(self->remotePort_);

      auto ret = sendto(self->videoRtpSock_, msg->data(), msg->size(), 0,
                        (struct sockaddr*)&addr, sizeof(addr));
    }
  });
}

void RtpClient::removeConnection(size_t ref) {
  NPLOGD << "Removing Nabto Connection from RTP";
  size_t mediaTracksSize = 0;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto it = mediaTracks_.begin(); it != mediaTracks_.end(); ++it) {
      if (it->ref == ref) {
        mediaTracks_.erase(it);
        break;
      }
    }
    mediaTracksSize = mediaTracks_.size();
  }
  if (mediaTracksSize == 0) {
    NPLOGD << "Connection was last one. Stopping";
    stop();
  } else {
    NPLOGD << "Still " << mediaTracksSize << " Connections. Not stopping";
  }
}

void RtpClient::start() {
  NPLOGI << "Starting RTP Client listen on port " << videoPort_;

  stopped_ = false;
  videoRtpSock_ = socket(AF_INET, SOCK_DGRAM, 0);
  struct sockaddr_in addr = {};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr("0.0.0.0");
  addr.sin_port = htons(videoPort_);
  if (bind(videoRtpSock_, reinterpret_cast<const sockaddr*>(&addr),
           sizeof(addr)) < 0) {
    std::string err = "Failed to bind UDP socket on 0.0.0.0:";
    err += std::to_string(videoPort_);
    NPLOGE << "Failed to bind RTP socket: " << err;
    throw std::runtime_error(err);
  }

  int rcvBufSize = 212992;
  setsockopt(videoRtpSock_, SOL_SOCKET, SO_RCVBUF,
             reinterpret_cast<const char*>(&rcvBufSize), sizeof(rcvBufSize));
  videoThread_ = std::thread(rtpVideoRunner, this);
}

void RtpClient::stop() {
  NPLOGD << "RtpClient stopped";
  bool stopped = false;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (stopped_) {
      stopped = stopped_;
    } else {
      stopped_ = true;
      if (videoRtpSock_ != 0) {
        shutdown(videoRtpSock_, SHUT_RDWR);
        close(videoRtpSock_);
      }
    }
  }
  if (!stopped && videoThread_.joinable()) {
    videoThread_.join();
  }
  mediaTracks_.clear();
  NPLOGD << "RtpClient thread joined";
}

void RtpClient::rtpVideoRunner(RtpClient* self) {
  char buffer[RTP_BUFFER_SIZE];
  int len;
  int count = 0;
  struct sockaddr_in srcAddr;
  socklen_t srcAddrLen = sizeof(srcAddr);
  while (true) {
    {
      std::lock_guard<std::mutex> lock(self->mutex_);
      if (self->stopped_) {
        break;
      }
    }

    len = recvfrom(self->videoRtpSock_, buffer, RTP_BUFFER_SIZE, 0,
                   (struct sockaddr*)&srcAddr, &srcAddrLen);

    if (len < 0) {
      break;
    }

    count++;
    if (count % 100 == 0) {
      std::cout << ".";
    }
    if (count % 1600 == 0) {
      std::cout << std::endl;
      count = 0;
    }
    if (len < sizeof(rtc::RtpHeader)) {
      continue;
    }

    {
      std::lock_guard<std::mutex> lock(self->mutex_);
      for (auto it = self->mediaTracks_.begin(); it != self->mediaTracks_.end();
           ++it) {
        if (it->track->isOpen()) {
          try {
            it->track->send((rtc::byte*)buffer, len);
          } catch (std::runtime_error& ex) {
            NPLOGE << "Failed to send on track: " << ex.what();
          }
        }
      }
    }
  }
}

}  // namespace example
}  // namespace nabto
