#include "lws_context_manager.hpp"

#include <plog/Log.h>

#include <cstring>

namespace nabto {
namespace webrtc {

std::weak_ptr<LwsContextManager> LwsContextManager::instance_;
std::mutex LwsContextManager::instanceMutex_;

// Combined protocols for both HTTP and WebSocket clients
static const struct lws_protocols protocols[] = {
    {"http", nullptr, 0, 0, 0, nullptr, 0},
    {"websocket", nullptr, 0, 0, 0, nullptr, 0},
    LWS_PROTOCOL_LIST_TERM
};

std::shared_ptr<LwsContextManager> LwsContextManager::getInstance() {
  std::lock_guard<std::mutex> lock(instanceMutex_);

  auto shared = instance_.lock();
  if (!shared) {
    // Create new instance using private constructor
    shared = std::shared_ptr<LwsContextManager>(new LwsContextManager());
    instance_ = shared;
  }

  return shared;
}

LwsContextManager::LwsContextManager()
    : context_(nullptr), refCount_(0), running_(false) {}

LwsContextManager::~LwsContextManager() {
  shutdownContext();
}

bool LwsContextManager::initContext() {
  std::lock_guard<std::mutex> lock(mutex_);

  if (context_) {
    // Already initialized
    return true;
  }

  struct lws_context_creation_info info;
  memset(&info, 0, sizeof(info));

  info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
  info.port = CONTEXT_PORT_NO_LISTEN;
  info.protocols = protocols;
  info.fd_limit_per_thread = 10;  // Allow multiple simultaneous connections
  info.gid = -1;
  info.uid = -1;

  context_ = lws_create_context(&info);
  if (!context_) {
    PLOG_ERROR << "Failed to create libwebsockets context";
    return false;
  }

  running_ = true;
  serviceThread_ = std::thread(&LwsContextManager::serviceLoop, this);

  PLOG_DEBUG << "LwsContextManager: Context initialized";
  return true;
}

void LwsContextManager::shutdownContext() {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!context_) {
    return;
  }

  PLOG_DEBUG << "LwsContextManager: Shutting down context";

  running_ = false;

  if (context_) {
    lws_cancel_service(context_);
  }

  if (serviceThread_.joinable()) {
    serviceThread_.join();
  }

  if (context_) {
    lws_context_destroy(context_);
    context_ = nullptr;
  }

  PLOG_DEBUG << "LwsContextManager: Context shut down";
}

void LwsContextManager::serviceLoop() {
  PLOG_DEBUG << "LwsContextManager: Service loop started";

  while (running_) {
    if (context_) {
      lws_service(context_, 50);
    }
  }

  PLOG_DEBUG << "LwsContextManager: Service loop ended";
}

struct lws_context* LwsContextManager::getContext() {
  std::lock_guard<std::mutex> lock(mutex_);
  return context_;
}

bool LwsContextManager::addRef() {
  int oldCount = refCount_.fetch_add(1);

  PLOG_DEBUG << "LwsContextManager: addRef() - ref count: " << oldCount << " -> " << (oldCount + 1);

  if (oldCount == 0) {
    // First reference, initialize context
    if (!initContext()) {
      refCount_.fetch_sub(1);
      return false;
    }
  }

  return true;
}

void LwsContextManager::removeRef() {
  int oldCount = refCount_.fetch_sub(1);

  PLOG_DEBUG << "LwsContextManager: removeRef() - ref count: " << oldCount << " -> " << (oldCount - 1);

  if (oldCount == 1) {
    // Last reference removed, shutdown context
    shutdownContext();
  }
}

void LwsContextManager::cancelService() {
  if (context_) {
    lws_cancel_service(context_);
  }
}

}  // namespace webrtc
}  // namespace nabto
