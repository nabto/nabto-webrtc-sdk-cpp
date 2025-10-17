#pragma once

#include <libwebsockets.h>

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <nabto/webrtc/device.hpp>
#include <nabto/webrtc/util/logging.hpp>
#include <queue>
#include <string>
#include <thread>

#include "lws_context_manager.hpp"

namespace nabto::example {

class LwsWebsocket : public nabto::webrtc::SignalingWebsocket {
 public:
  static nabto::webrtc::SignalingWebsocketPtr create() {
    return std::make_shared<LwsWebsocket>();
  }

  static int lwsCallback(struct lws* wsi, enum lws_callback_reasons reason,
                         void* user, void* in, size_t len);

  ~LwsWebsocket() override;

  bool send(const std::string& data) override;
  void close() override;
  void onOpen(std::function<void()> callback) override;
  void onMessage(
      std::function<void(const std::string& message)> callback) override;
  void onClosed(std::function<void()> callback) override;
  void onError(std::function<void(const std::string& error)> callback) override;
  void open(const std::string& url) override;

 private:
  void cleanup();

  std::shared_ptr<LwsContextManager> contextManager_ =
      LwsContextManager::getInstance();
  struct lws* wsi_ = nullptr;
  std::atomic<bool> connected_{false};

  // Callbacks
  std::function<void()> onOpen_;
  std::function<void(const std::string&)> onMessage_;
  std::function<void()> onClosed_;
  std::function<void(const std::string&)> onError_;

  // Send queue
  std::queue<std::string> sendQueue_;
  std::vector<unsigned char> writeBuffer_;
  std::mutex queueMutex_;
  std::mutex callbackMutex_;
};

}  // namespace nabto::example