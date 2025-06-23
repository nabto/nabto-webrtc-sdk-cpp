#pragma once

#include <nabto/webrtc/device.hpp>

#include <chrono>
#include <thread>

namespace nabto {
namespace util {

/**
 * Implementation of the SignalingTimer interface used by the SDK. This is based
 * on std thread and std chrono libs.
 */
class StdTimer : public nabto::signaling::SignalingTimer,
                 public std::enable_shared_from_this<StdTimer> {
 public:
  ~StdTimer() {}

  void setTimeout(uint32_t timeoutMs, std::function<void()> cb) override {
    auto self = shared_from_this();
    timer_ = std::thread([timeoutMs, self, cb]() {
      std::this_thread::sleep_for(std::chrono::milliseconds(timeoutMs));
      // TODO: thread safety
      cb();
    });
  }

  void cancel() override {
    if (timer_.joinable()) {
      timer_.join();
      timer_ = std::thread();
    }
  }

 private:
  std::thread timer_;
};

/**
 * Implementation of the SignalingTimerFactory interface used by the SDK. This
 * is used to create timers using the StdTimer implementation.
 */
class StdTimerFactory : public nabto::signaling::SignalingTimerFactory {
 public:
  /**
   * Create a SignalingTimerFactory instance.
   *
   * @return SignalingTimerFactoryPtr to the resulting timer factory.
   */
  static nabto::signaling::SignalingTimerFactoryPtr create() {
    return std::make_shared<StdTimerFactory>();
  }

  nabto::signaling::SignalingTimerPtr createTimer() override {
    return std::make_shared<StdTimer>();
  }
};

}  // namespace util
}  // namespace nabto
