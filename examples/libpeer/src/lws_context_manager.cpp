#include <nabto/webrtc/util/logging.hpp>
#include "lws_context_manager.hpp"
#include "lws_websocket.hpp"
#include "lws_http_client.hpp"

#include <plog/Log.h>

#include <cstring>

namespace nabto::example {

struct lws_protocols lwsProtocols[] = {
  {
    "lws-websocket-protocol",
    LwsWebsocket::lwsCallback,
    0,
    0
  },
  {
    "lws-http-protocol",
    LwsHttpClient::lwsCallback,
    0,
    0
  },
  { nullptr, nullptr, 0, 0 }
};

std::weak_ptr<LwsContextManager> LwsContextManager::instance_;
std::mutex LwsContextManager::instanceMutex_;

std::shared_ptr<LwsContextManager> LwsContextManager::getInstance() {
  std::lock_guard<std::mutex> lock(instanceMutex_);

  auto shared = instance_.lock();
  if (!shared) {
    shared = std::shared_ptr<LwsContextManager>(new LwsContextManager());
    instance_ = shared;
  }
  return shared;
}

LwsContextManager::LwsContextManager() {
  struct lws_context_creation_info info = {};
  info.port = CONTEXT_PORT_NO_LISTEN;
  info.protocols = lwsProtocols;
  info.gid = -1;
  info.uid = -1;
  info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;

  context_ = lws_create_context(&info);

  if (context_) {
    running_ = true;
    serviceThread_ = std::thread(&LwsContextManager::serviceLoop, this);
  }
}

LwsContextManager::~LwsContextManager() {
  cleanup();
}

void LwsContextManager::serviceLoop() {
  int n = 0;
  while (running_) {
    if (context_) {
      n = lws_service(context_, 50);
    }
  }
}

void LwsContextManager::cleanup() {
  running_ = false;

  if (serviceThread_.joinable()) {
    serviceThread_.join();
  }
  if (context_) {
    lws_context_destroy(context_);
    context_ = nullptr;
  }
}

}  // namespace nabto::example
