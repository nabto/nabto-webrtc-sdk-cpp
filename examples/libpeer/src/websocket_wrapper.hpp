#pragma once

#include <libwebsockets.h>
#include <string>
#include <functional>
#include <queue>
#include <mutex>
#include <atomic>
#include <memory>
#include <thread>

#include <nabto/webrtc/device.hpp>
#include <nabto/webrtc/util/logging.hpp>

namespace nabto {
namespace webrtc {
class LwsContextManager;
}
}

namespace nabto::example {

class LibwebsocketsSignalingWebsocket : 
  public nabto::webrtc::SignalingWebsocket,
  public std::enable_shared_from_this<LibwebsocketsSignalingWebsocket> {
 public:
  static nabto::webrtc::SignalingWebsocketPtr create() {
    return std::make_shared<LibwebsocketsSignalingWebsocket>();
  }

  LibwebsocketsSignalingWebsocket();
  ~LibwebsocketsSignalingWebsocket() override;

  bool send(const std::string& data) override;
  void close() override;
  void onOpen(std::function<void()> callback) override;
  void onMessage(std::function<void(const std::string& message)> callback) override;
  void onClosed(std::function<void()> callback) override;
  void onError(std::function<void(const std::string& error)> callback) override;
  void open(const std::string& url) override;

 private:
  static int websocketCallback(struct lws* wsi, enum lws_callback_reasons reason,
                               void* user, void* in, size_t len);
  
  void serviceLoop();
  void cleanup();

  struct lws_context* context_ = nullptr;
  struct lws* wsi_ = nullptr;
  std::thread service_thread_;
  std::atomic<bool> running_{false};
  std::atomic<bool> connected_{false};

  // Callbacks
  std::function<void()> on_open_;
  std::function<void(const std::string&)> on_message_;
  std::function<void()> on_closed_;
  std::function<void(const std::string&)> on_error_;

  // Send queue
  std::queue<std::string> send_queue_;
  std::mutex queue_mutex_;
  std::mutex callback_mutex_;
};

} // nabto::example