#pragma once

#include <nabto/webrtc/device.hpp>

#include <chrono>
#include <thread>

namespace nabto {
namespace util {

class StdTimer : public nabto::signaling::SignalingTimer,
                 public std::enable_shared_from_this<StdTimer> {
 public:
  ~StdTimer() {
    if (timer_.joinable()) {
      timer_.join();
    }
  }

  void setTimeout(uint32_t timeoutMs, std::function<void()> cb) override {
    auto self = shared_from_this();
    timer_ = std::thread([timeoutMs, self, cb]() {
      std::this_thread::sleep_for(std::chrono::milliseconds(timeoutMs));
      // TODO: thread safety
      cb();
    });
  }

 private:
  std::thread timer_;
};

class StdTimerFactory : public nabto::signaling::SignalingTimerFactory {
 public:
  static nabto::signaling::SignalingTimerFactoryPtr create() {
    return std::make_shared<StdTimerFactory>();
  }

  nabto::signaling::SignalingTimerPtr createTimer() override {
    return std::make_shared<StdTimer>();
  }
};

}  // namespace util
}  // namespace nabto
