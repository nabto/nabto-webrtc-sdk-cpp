
#pragma once

#include <memory>

namespace nabto {
namespace example {

class WebrtcTrackHandler;

typedef std::shared_ptr<WebrtcTrackHandler> WebrtcTrackHandlerPtr;

class WebrtcTrackHandler {
 public:
  virtual size_t addTrack(void) = 0;
  virtual void removeConnection(size_t ref) = 0;
};

}  // namespace example
}  // namespace nabto
