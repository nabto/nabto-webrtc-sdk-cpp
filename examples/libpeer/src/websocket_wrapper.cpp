#include <nabto/webrtc/util/logging.hpp>
#include "websocket_wrapper.hpp"

namespace nabto::example {

LwsWebsocket::~LwsWebsocket() {
  cleanup();
}

void LwsWebsocket::open(const std::string& url) {
  if (!contextManager_ || !contextManager_->getContext()) {
    if (onError_) {
      onError_("Failed to get LWS context");
    }
    return;
  }

  // Parse URL
  std::string protocol, host, path;
  int port = 443;
  bool use_ssl = true;

  size_t proto_end = url.find("://");
  if (proto_end != std::string::npos) {
    protocol = url.substr(0, proto_end);
    use_ssl = (protocol == "wss");
    
    size_t host_start = proto_end + 3;
    size_t path_start = url.find('/', host_start);
    size_t port_start = url.find(':', host_start);
    
    if (port_start != std::string::npos && 
        (path_start == std::string::npos || port_start < path_start)) {
      host = url.substr(host_start, port_start - host_start);
      size_t port_end = (path_start != std::string::npos) ? path_start : url.length();
      port = std::stoi(url.substr(port_start + 1, port_end - port_start - 1));
    } else if (path_start != std::string::npos) {
      host = url.substr(host_start, path_start - host_start);
    } else {
      host = url.substr(host_start);
    }
    
    if (path_start != std::string::npos) {
      path = url.substr(path_start);
    } else {
      path = "/";
    }
  }

  if (!use_ssl) {
    port = (port == 443) ? 80 : port;
  }

  // setup connection info
  struct lws_client_connect_info ccinfo = {};
  ccinfo.context = contextManager_->getContext();
  ccinfo.address = host.c_str();
  ccinfo.port = port;
  ccinfo.path = path.c_str();
  ccinfo.host = host.c_str();
  ccinfo.origin = host.c_str();
  ccinfo.protocol = "lws-websocket-protocol";
  ccinfo.ssl_connection = LCCSCF_USE_SSL;
  ccinfo.userdata = this;

  wsi_ = lws_client_connect_via_info(&ccinfo);
  if (!wsi_) {
    if (onError_) {
      onError_("Failed to connect to websocket");
    }
    return;
  }

  NPLOGI << "Websocket connected succesfully";
  contextManager_->registerWebsocket(wsi_);
}

bool LwsWebsocket::send(const std::string& data) {
  NPLOGI << data;
  if (!connected_ || !wsi_) {
    return false;
  }

  std::lock_guard<std::mutex> lock(queueMutex_);
  sendQueue_.push(data);

  if (contextManager_ && contextManager_->getContext()) {
    lws_callback_on_writable(wsi_);
  }

  return true;
}

void LwsWebsocket::close() {
  if (wsi_) {
    lws_callback_on_writable(wsi_);
  }

  cleanup();
}

void LwsWebsocket::cleanup() {
  if (wsi_ && contextManager_) {
    contextManager_->unregisterWebsocket(wsi_);
    wsi_ = nullptr;
  }
  
  connected_ = false;
}

void LwsWebsocket::onMessage(std::function<void(const std::string& message)> callback) {
  std::lock_guard<std::mutex> lock(callbackMutex_);
  onMessage_ = callback;
}

void LwsWebsocket::onClosed(std::function<void()> callback) {
  std::lock_guard<std::mutex> lock(callbackMutex_);
  onClosed_ = callback;
}

void LwsWebsocket::onOpen(std::function<void()> callback) {
  std::lock_guard<std::mutex> lock(callbackMutex_);
  onOpen_ = callback;
}

void LwsWebsocket::onError(std::function<void(const std::string& error)> callback) {
  std::lock_guard<std::mutex> lock(callbackMutex_);
  onError_ = callback;
}

int LwsWebsocket::websocketCallback(
  struct lws* wsi,
  enum lws_callback_reasons reason,
  void* userdata,
  void* in,
  size_t len
) {

  auto* ws = static_cast<LwsWebsocket*>(userdata);
  if (!ws) {
    return 0;
  }

  switch (reason) {
    case LWS_CALLBACK_CLIENT_ESTABLISHED: {
      ws->connected_ = true;
      {
        std::lock_guard<std::mutex> lock(ws->callbackMutex_);
        if (ws->onOpen_) {
          ws->onOpen_();
        }
      }
      break;
    }

    case LWS_CALLBACK_CLIENT_RECEIVE: {
      std::string message(static_cast<const char*>(in), len);
      NPLOGI << message;
      std::lock_guard<std::mutex> lock(ws->callbackMutex_);
      if (ws->onMessage_) {
        ws->onMessage_(message);
      }
      break;
    }

    case LWS_CALLBACK_CLIENT_WRITEABLE: {
      std::lock_guard<std::mutex> lock(ws->queueMutex_);
      if (!ws->sendQueue_.empty()) {
        std::string& msg = ws->sendQueue_.front();
        size_t writeSize = msg.size();

        if (ws->writeBuffer_.size() < (LWS_PRE + writeSize)) {
          ws->writeBuffer_.resize(LWS_PRE + writeSize);
        }

        memcpy(&ws->writeBuffer_[LWS_PRE], msg.c_str(), writeSize);

        lws_write(wsi, &ws->writeBuffer_[LWS_PRE], writeSize, LWS_WRITE_TEXT);
        ws->sendQueue_.pop();

        if (!ws->sendQueue_.empty()) {
          lws_callback_on_writable(wsi);
        }
      }
      break;
    }

    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR: {
      std::string error = in ? std::string(static_cast<const char*>(in), len) : "Connection error";
      std::lock_guard<std::mutex> lock(ws->callbackMutex_);
      if (ws->onError_) {
        ws->onError_(error);
      }
      ws->connected_ = false;
      break;
    }

    case LWS_CALLBACK_CLOSED: {
      ws->connected_ = false;
      {
        std::lock_guard<std::mutex> lock(ws->callbackMutex_);
        if (ws->onClosed_) {
          ws->onClosed_();
        }
      }
      break;
    }

    default: break;
  }

  return 0;
}

}