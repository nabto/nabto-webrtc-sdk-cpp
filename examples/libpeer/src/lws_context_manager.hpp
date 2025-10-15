#pragma once

#include <libwebsockets.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

namespace nabto::example {

class LwsContextManager {
public:
  static std::shared_ptr<LwsContextManager> getInstance();
  ~LwsContextManager();

  struct lws_context* getContext() { return context_; }

  void registerWebsocket(struct lws* wsi);
  void unregisterWebsocket(struct lws* wsi);
private:
  LwsContextManager();
  void serviceLoop();
  void cleanup();

  struct lws_context* context_ = nullptr;
  std::thread serviceThread_;
  std::atomic<bool> running_{false};
  std::mutex mutex_;
  int activeConnections_ = 0;

  static std::weak_ptr<LwsContextManager> instance_;
  static std::mutex instanceMutex_;
};

} // nabto::example

