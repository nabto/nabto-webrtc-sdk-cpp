#pragma once

#include <sys/socket.h>

#include "rtp_track.hpp"

typedef int SOCKET;

#include <memory>
#include <thread>

namespace nabto {
namespace example {

class RtpClient;
typedef std::shared_ptr<RtpClient> RtpClientPtr;

class RtpClientConf {
   public:
    std::string remoteHost;
    uint16_t port = 0;
};

class RtpClient : public std::enable_shared_from_this<RtpClient> {
   public:
    static RtpClientPtr create(const RtpClientConf& conf);
    RtpClient(const RtpClientConf& conf);

    ~RtpClient();

    size_t addConnection(std::shared_ptr<rtc::Track> track, const rtc::SSRC ssrc, int payloadType);
    void removeConnection(size_t ref);

   private:
    void start();
    void stop();
    void addConnection(RtpTrack track);
    static void rtpVideoRunner(RtpClient* self);

    std::string trackId_;
    bool stopped_ = true;
    std::mutex mutex_;
    size_t index_ = 0;

    std::vector<RtpTrack> mediaTracks_;

    uint16_t videoPort_ = 6000;
    uint16_t remotePort_ = 6002;
    std::string remoteHost_ = "127.0.0.1";
    SOCKET videoRtpSock_ = 0;
    std::thread videoThread_;
};

}  // namespace example
}  // namespace nabto
