#pragma once

#include <libwebsockets.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

namespace nabto {
namespace webrtc {

/**
 * Shared libwebsockets context manager.
 * This class manages a single lws_context that can be shared between
 * multiple clients (HTTP, WebSocket, etc.) to avoid duplicate contexts
 * and service threads.
 */
class LwsContextManager : public std::enable_shared_from_this<LwsContextManager> {
 public:
  static std::shared_ptr<LwsContextManager> getInstance();

  ~LwsContextManager();

  // Delete copy and move constructors/operators
  LwsContextManager(const LwsContextManager&) = delete;
  LwsContextManager& operator=(const LwsContextManager&) = delete;
  LwsContextManager(LwsContextManager&&) = delete;
  LwsContextManager& operator=(LwsContextManager&&) = delete;

  /**
   * Get the managed lws_context.
   * Returns nullptr if context is not initialized.
   */
  struct lws_context* getContext();

  /**
   * Increment the reference count and ensure context is running.
   * @return true if successful, false otherwise
   */
  bool addRef();

  /**
   * Decrement the reference count. When it reaches zero,
   * the context will be stopped and destroyed.
   */
  void removeRef();

  /**
   * Cancel the service loop (useful for waking up the service thread)
   */
  void cancelService();

 private:
  LwsContextManager();

  bool initContext();
  void shutdownContext();
  void serviceLoop();

  static std::weak_ptr<LwsContextManager> instance_;
  static std::mutex instanceMutex_;

  struct lws_context* context_;
  std::atomic<int> refCount_;
  std::atomic<bool> running_;
  std::thread serviceThread_;
  std::mutex mutex_;
};

using LwsContextManagerPtr = std::shared_ptr<LwsContextManager>;

}  // namespace webrtc
}  // namespace nabto
