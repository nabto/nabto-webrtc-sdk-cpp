#pragma once

#include <chrono>
#include <thread>

#include "nabto/signaling/signaling.hpp"

namespace nabto {
namespace example {

class StdTimer : public nabto::signaling::SignalingTimer, public std::enable_shared_from_this<StdTimer> {
   public:
    ~StdTimer() {
        if (timer_.joinable()) {
            timer_.join();
        }
    }

    void setTimeout(uint32_t timeoutMs, std::function<void()> cb) {
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

    nabto::signaling::SignalingTimerPtr createTimer() {
        return std::make_shared<StdTimer>();
    }
};

}  // namespace example
}  // namespace nabto
