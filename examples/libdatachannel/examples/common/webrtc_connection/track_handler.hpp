
#pragma once

#include <memory>
#include <rtc/rtc.hpp>

namespace nabto {
namespace example {

class WebrtcTrackHandler;

typedef std::shared_ptr<WebrtcTrackHandler> WebrtcTrackHandlerPtr;

class WebrtcTrackHandler
{
  public:
    virtual size_t addTrack(std::shared_ptr<rtc::PeerConnection> pc) = 0;
    virtual void removeConnection(size_t ref) = 0;
};

}} // namespaces
