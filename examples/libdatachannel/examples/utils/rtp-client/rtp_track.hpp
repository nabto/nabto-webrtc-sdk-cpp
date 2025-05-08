
#pragma once

#include <memory>
#include <rtc/rtc.hpp>

namespace nabto {
namespace example {

class RtpTrack {
   public:
    size_t ref;
    std::shared_ptr<rtc::Track> track;
    rtc::SSRC ssrc;
    int srcPayloadType = 0;
    int dstPayloadType = 0;
};

}  // namespace example
}  // namespace nabto
