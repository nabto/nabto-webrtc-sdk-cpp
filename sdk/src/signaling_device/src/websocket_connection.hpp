#pragma once

#include "signaling_impl.hpp"

#include <nabto/webrtc/device.hpp>

#include <nlohmann/json.hpp>

#include <cstddef>
#include <functional>
#include <memory>
#include <string>

namespace nabto {
namespace signaling {

class WebsocketConnection;
using WebsocketConnectionPtr = std::shared_ptr<WebsocketConnection>;

class WebsocketConnection
    : public std::enable_shared_from_this<WebsocketConnection> {
 public:
  static WebsocketConnectionPtr create(SignalingWebsocketPtr websocket,
                                       SignalingTimerFactoryPtr timerFactory) {
    return std::make_shared<WebsocketConnection>(std::move(websocket),
                                                 std::move(timerFactory));
  }

  WebsocketConnection(SignalingWebsocketPtr websocket,
                      SignalingTimerFactoryPtr timerFactory)
      : ws_(std::move(websocket)), timerFactory_(std::move(timerFactory)) {}

  bool send(const std::string& data);
  void close();
  void onOpen(std::function<void()> callback);
  void onMessage(const std::function<void(SignalingMessageType type,
                                          nlohmann::json& message)>& callback);
  void onClosed(std::function<void()> callback);
  void onError(std::function<void(const std::string& error)> callback);

  void open(const std::string& url);

  void checkAlive();

 private:
  SignalingWebsocketPtr ws_;
  SignalingTimerFactoryPtr timerFactory_;
  size_t pongCounter_ = 0;
  SignalingTimerPtr timer_;

  void handlePong();
  static SignalingMessageType parseWsMsgType(const std::string& str);
};

}  // namespace signaling
}  // namespace nabto
