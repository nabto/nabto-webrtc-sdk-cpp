#include "websocket_wrapper.hpp"

namespace nabto::example {

LibwebsocketsSignalingWebsocket::LibwebsocketsSignalingWebsocket() = default;

LibwebsocketsSignalingWebsocket::~LibwebsocketsSignalingWebsocket() {
  cleanup();
}

void LibwebsocketsSignalingWebsocket::open(const std::string& url) {
  if (running_) {
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

  // Create context
  struct lws_context_creation_info info;
  memset(&info, 0, sizeof(info));
  info.port = CONTEXT_PORT_NO_LISTEN;
  info.protocols = nullptr;  // Using default protocols
  info.gid = -1;
  info.uid = -1;
  info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
  info.user = this;

  context_ = lws_create_context(&info);
  if (!context_) {
    if (on_error_) {
      on_error_("Failed to create libwebsockets context");
    }
    return;
  }

  // Setup client connection info
  struct lws_client_connect_info ccinfo;
  memset(&ccinfo, 0, sizeof(ccinfo));
  ccinfo.context = context_;
  ccinfo.address = host.c_str();
  ccinfo.port = port;
  ccinfo.path = path.c_str();
  ccinfo.host = host.c_str();
  ccinfo.origin = host.c_str();
  ccinfo.protocol = nullptr;
  ccinfo.ssl_connection = use_ssl ? LCCSCF_USE_SSL : 0;
  ccinfo.userdata = this;

  wsi_ = lws_client_connect_via_info(&ccinfo);
  if (!wsi_) {
    if (on_error_) {
      on_error_("Failed to connect to websocket");
    }
    cleanup();
    return;
  }

  // Start service thread
  running_ = true;
  service_thread_ = std::thread(&LibwebsocketsSignalingWebsocket::serviceLoop, this);
}

void LibwebsocketsSignalingWebsocket::serviceLoop() {
  while (running_) {
    if (context_) {
      lws_service(context_, 50);
    }
    
    // Process send queue
    std::lock_guard<std::mutex> lock(queue_mutex_);
    if (!send_queue_.empty() && connected_ && wsi_) {
      lws_callback_on_writable(wsi_);
    }
  }
}

bool LibwebsocketsSignalingWebsocket::send(const std::string& data) {
  if (!connected_ || !wsi_) {
    return false;
  }

  std::lock_guard<std::mutex> lock(queue_mutex_);
  send_queue_.push(data);
  
  if (context_) {
    lws_callback_on_writable(wsi_);
  }
  
  return true;
}

void LibwebsocketsSignalingWebsocket::close() {
  running_ = false;
  
  if (wsi_) {
    lws_callback_on_writable(wsi_);
  }
  
  cleanup();
}

void LibwebsocketsSignalingWebsocket::cleanup() {
  running_ = false;
  
  if (service_thread_.joinable()) {
    service_thread_.join();
  }
  
  if (context_) {
    lws_context_destroy(context_);
    context_ = nullptr;
  }
  
  wsi_ = nullptr;
  connected_ = false;
}

void LibwebsocketsSignalingWebsocket::onOpen(std::function<void()> callback) {
  std::lock_guard<std::mutex> lock(callback_mutex_);
  on_open_ = callback;
}

void LibwebsocketsSignalingWebsocket::onMessage(
    std::function<void(const std::string& message)> callback) {
  std::lock_guard<std::mutex> lock(callback_mutex_);
  on_message_ = callback;
}

void LibwebsocketsSignalingWebsocket::onClosed(std::function<void()> callback) {
  std::lock_guard<std::mutex> lock(callback_mutex_);
  on_closed_ = callback;
}

void LibwebsocketsSignalingWebsocket::onError(
    std::function<void(const std::string& error)> callback) {
  std::lock_guard<std::mutex> lock(callback_mutex_);
  on_error_ = callback;
}

int LibwebsocketsSignalingWebsocket::websocketCallback(
    struct lws* wsi, enum lws_callback_reasons reason,
    void* user, void* in, size_t len) {
  
  auto* ws = static_cast<LibwebsocketsSignalingWebsocket*>(
      lws_context_user(lws_get_context(wsi)));
  
  if (!ws) {
    return 0;
  }

  switch (reason) {
    case LWS_CALLBACK_CLIENT_ESTABLISHED:
      ws->connected_ = true;
      {
        std::lock_guard<std::mutex> lock(ws->callback_mutex_);
        if (ws->on_open_) {
          ws->on_open_();
        }
      }
      break;

    case LWS_CALLBACK_CLIENT_RECEIVE:
      {
        std::string message(static_cast<const char*>(in), len);
        std::lock_guard<std::mutex> lock(ws->callback_mutex_);
        if (ws->on_message_) {
          ws->on_message_(message);
        }
      }
      break;

    case LWS_CALLBACK_CLIENT_WRITEABLE:
      {
        std::lock_guard<std::mutex> lock(ws->queue_mutex_);
        if (!ws->send_queue_.empty()) {
          std::string& msg = ws->send_queue_.front();
          size_t write_size = msg.size();
          
          unsigned char buf[LWS_PRE + write_size];
          memcpy(&buf[LWS_PRE], msg.c_str(), write_size);
          
          lws_write(wsi, &buf[LWS_PRE], write_size, LWS_WRITE_TEXT);
          ws->send_queue_.pop();
          
          if (!ws->send_queue_.empty()) {
            lws_callback_on_writable(wsi);
          }
        }
      }
      break;

    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
      {
        std::string error = in ? std::string(static_cast<const char*>(in), len) 
                               : "Connection error";
        std::lock_guard<std::mutex> lock(ws->callback_mutex_);
        if (ws->on_error_) {
          ws->on_error_(error);
        }
      }
      ws->connected_ = false;
      break;

    case LWS_CALLBACK_CLOSED:
      ws->connected_ = false;
      {
        std::lock_guard<std::mutex> lock(ws->callback_mutex_);
        if (ws->on_closed_) {
          ws->on_closed_();
        }
      }
      break;

    default:
      break;
  }

  return 0;
}

}