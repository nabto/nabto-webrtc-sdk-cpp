#pragma once

#include <nabto/webrtc/device.hpp>
#include <nabto/webrtc/util/logging.hpp>
#include <rtc/rtc.hpp>
#include <variant>

namespace nabto {
namespace example {

class RtcWebsocketWrapper
    : public nabto::webrtc::SignalingWebsocket,
      public std::enable_shared_from_this<RtcWebsocketWrapper> {
 public:
  static nabto::webrtc::SignalingWebsocketPtr create() {
    return std::make_shared<RtcWebsocketWrapper>();
  }

  RtcWebsocketWrapper() {}

  bool send(const std::string& data) { return ws_.send(data); }

  void close() { return ws_.close(); }

  void onOpen(std::function<void()> callback) { ws_.onOpen(callback); }

  void onMessage(std::function<void(const std::string& message)> callback) {
    auto self = shared_from_this();
    ws_.onMessage(
        [self, callback](std::variant<rtc::binary, rtc::string> message) {
          std::string msg;
          if (std::holds_alternative<rtc::string>(message)) {
            msg = std::get<rtc::string>(message);
            return callback(msg);
          } else {
            // TODO: support binary messages
            NPLOGE << "GOT BINARY MESSAGE FIX HANDLE";
            return;
            // auto bin = std::get<rtc::binary>(message);
            // msg = std::string(bin.begin(), bin.end());
          }
        });
  }

  void onClosed(std::function<void()> callback) { ws_.onClosed(callback); }

  void open(const std::string& url) { ws_.open(url); }

  void onError(std::function<void(const std::string& error)> callback) {
    ws_.onError(callback);
  }

 private:
  rtc::WebSocket ws_;
};

}  // namespace example
}  // namespace nabto
