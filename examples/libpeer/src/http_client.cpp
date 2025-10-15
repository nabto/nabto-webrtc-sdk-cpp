#include "http_client.hpp"

#include <plog/Log.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <sstream>

namespace nabto {
namespace webrtc {

namespace {
// Protocol structures for libwebsockets HTTP client
static const struct lws_protocols protocols[] = {
    {"http", LwsHttpClient::lwsCallback, 0, 0, 0, nullptr, 0},
    LWS_PROTOCOL_LIST_TERM};
}  // namespace

SignalingHttpClientPtr LwsHttpClient::create() {
  auto client = std::make_shared<LwsHttpClient>();
  if (!client->init()) {
    return nullptr;
  }
  return client;
}

LwsHttpClient::LwsHttpClient() : running_(false) {}

LwsHttpClient::~LwsHttpClient() { stop(); }

bool LwsHttpClient::init() {
  contextManager_ = LwsContextManager::getInstance();
  if (!contextManager_) {
    PLOG_ERROR << "Failed to get LwsContextManager instance";
    return false;
  }

  if (!contextManager_->addRef()) {
    PLOG_ERROR << "Failed to initialize shared libwebsockets context";
    contextManager_.reset();
    return false;
  }

  running_ = true;
  return true;
}

void LwsHttpClient::stop() {
  if (!running_) {
    return;
  }

  running_ = false;

  {
    std::lock_guard<std::mutex> lock(mutex_);
    // Complete any pending requests with errors
    for (auto& pair : requests_) {
      if (pair.second && pair.second->callback) {
        auto response = std::make_unique<SignalingHttpResponse>();
        response->statusCode = 0;
        response->body = "Client stopped";
        pair.second->callback(std::move(response));
      }
    }
    requests_.clear();
  }

  if (contextManager_) {
    contextManager_->removeRef();
    contextManager_.reset();
  }
}


bool LwsHttpClient::parseUrl(const std::string& url, std::string& host,
                              std::string& path, int& port, bool& useTls) {
  // Simple URL parser for http:// and https://
  size_t schemeEnd = url.find("://");
  if (schemeEnd == std::string::npos) {
    return false;
  }

  std::string scheme = url.substr(0, schemeEnd);
  std::transform(scheme.begin(), scheme.end(), scheme.begin(),
                 [](unsigned char c) { return std::tolower(c); });

  if (scheme == "https") {
    useTls = true;
    port = 443;
  } else if (scheme == "http") {
    useTls = false;
    port = 80;
  } else {
    return false;
  }

  size_t hostStart = schemeEnd + 3;
  size_t pathStart = url.find('/', hostStart);
  size_t portStart = url.find(':', hostStart);

  if (pathStart == std::string::npos) {
    pathStart = url.length();
    path = "/";
  } else {
    path = url.substr(pathStart);
  }

  if (portStart != std::string::npos && portStart < pathStart) {
    host = url.substr(hostStart, portStart - hostStart);
    std::string portStr =
        url.substr(portStart + 1, pathStart - portStart - 1);
    port = std::stoi(portStr);
  } else {
    host = url.substr(hostStart, pathStart - hostStart);
  }

  return true;
}

bool LwsHttpClient::sendRequest(const SignalingHttpRequest& request,
                                 HttpResponseCallback cb) {
  if (!running_ || !contextManager_) {
    return false;
  }

  struct lws_context* context = contextManager_->getContext();
  if (!context) {
    return false;
  }

  auto ctx = std::make_shared<RequestContext>();
  ctx->url = request.url;
  ctx->method = request.method;
  ctx->headers = request.headers;
  ctx->body = request.body;
  ctx->callback = cb;
  ctx->statusCode = 0;
  ctx->headersSent = false;
  ctx->bodyPos = 0;
  ctx->responseComplete = false;

  if (!parseUrl(request.url, ctx->host, ctx->path, ctx->port, ctx->useTls)) {
    PLOG_ERROR << "Failed to parse URL: " << request.url;
    return false;
  }

  struct lws_client_connect_info ccinfo;
  memset(&ccinfo, 0, sizeof(ccinfo));

  ccinfo.context = context;
  ccinfo.port = ctx->port;
  ccinfo.address = ctx->host.c_str();
  ccinfo.path = ctx->path.c_str();
  ccinfo.host = ctx->host.c_str();
  ccinfo.origin = ctx->host.c_str();
  ccinfo.protocol = protocols[0].name;
  ccinfo.userdata = ctx.get();

  if (ctx->useTls) {
    ccinfo.ssl_connection = LCCSCF_USE_SSL;
  }

  struct lws* wsi = lws_client_connect_via_info(&ccinfo);
  if (!wsi) {
    PLOG_ERROR << "Failed to connect to " << request.url;
    return false;
  }

  ctx->wsi = wsi;

  {
    std::lock_guard<std::mutex> lock(mutex_);
    requests_[wsi] = ctx;
  }

  return true;
}

int LwsHttpClient::lwsCallback(struct lws* wsi,
                                enum lws_callback_reasons reason, void* user,
                                void* in, size_t len) {
  LwsHttpClient* client = nullptr;
  struct lws_context* context = lws_get_context(wsi);

  // Find the client instance from the context
  // Note: We need to get the client instance somehow. One way is to store it
  // in the context user data during initialization.

  std::shared_ptr<RequestContext> ctx;

  // Try to get the request context from the wsi
  if (user) {
    ctx = *static_cast<std::shared_ptr<RequestContext>*>(user);
  }

  switch (reason) {
    case LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER: {
      if (!ctx) break;

      unsigned char** p =
          reinterpret_cast<unsigned char**>(in);
      unsigned char* end = (*p) + len;

      // Add custom headers
      for (const auto& header : ctx->headers) {
        if (lws_add_http_header_by_name(
                wsi,
                reinterpret_cast<const unsigned char*>(header.first.c_str()),
                reinterpret_cast<const unsigned char*>(header.second.c_str()),
                header.second.length(), p, end)) {
          return -1;
        }
      }

      // Add Content-Length if we have a body
      if (!ctx->body.empty()) {
        std::string contentLength = std::to_string(ctx->body.length());
        if (lws_add_http_header_by_name(
                wsi,
                reinterpret_cast<const unsigned char*>("Content-Length"),
                reinterpret_cast<const unsigned char*>(
                    contentLength.c_str()),
                contentLength.length(), p, end)) {
          return -1;
        }
      }
      break;
    }

    case LWS_CALLBACK_CLIENT_HTTP_WRITEABLE: {
      if (!ctx) break;

      if (!ctx->headersSent) {
        // Send request with method
        int flags = LWS_WRITE_HTTP_HEADERS;
        if (!ctx->body.empty()) {
          flags |= LWS_WRITE_HTTP_HEADERS_CONTINUATION;
        }

        std::string requestLine = ctx->method + " " + ctx->path + " HTTP/1.1\r\n";

        if (lws_write(wsi,
                     reinterpret_cast<unsigned char*>(
                         const_cast<char*>(requestLine.c_str())),
                     requestLine.length(),
                     static_cast<enum lws_write_protocol>(flags)) < 0) {
          return -1;
        }

        ctx->headersSent = true;

        if (!ctx->body.empty()) {
          lws_callback_on_writable(wsi);
        }
      } else if (ctx->bodyPos < ctx->body.length()) {
        // Send body
        size_t remaining = ctx->body.length() - ctx->bodyPos;
        size_t toSend = std::min(remaining, size_t(4096));

        int flags = LWS_WRITE_HTTP;
        if (ctx->bodyPos + toSend >= ctx->body.length()) {
          flags = LWS_WRITE_HTTP_FINAL;
        }

        int written = lws_write(
            wsi,
            reinterpret_cast<unsigned char*>(
                const_cast<char*>(ctx->body.c_str() + ctx->bodyPos)),
            toSend, static_cast<enum lws_write_protocol>(flags));

        if (written < 0) {
          return -1;
        }

        ctx->bodyPos += written;

        if (ctx->bodyPos < ctx->body.length()) {
          lws_callback_on_writable(wsi);
        }
      }
      break;
    }

    case LWS_CALLBACK_ESTABLISHED_CLIENT_HTTP: {
      if (!ctx) break;
      ctx->statusCode = lws_http_client_http_response(wsi);
      break;
    }

    case LWS_CALLBACK_RECEIVE_CLIENT_HTTP_READ: {
      if (!ctx) break;
      ctx->responseBody.append(static_cast<const char*>(in), len);
      break;
    }

    case LWS_CALLBACK_RECEIVE_CLIENT_HTTP: {
      if (!ctx) break;

      char buffer[1024 + LWS_PRE];
      char* px = buffer + LWS_PRE;
      int lenx = sizeof(buffer) - LWS_PRE;

      // Read any remaining data
      if (lws_http_client_read(wsi, &px, &lenx) < 0) {
        return -1;
      }

      if (lenx > 0) {
        ctx->responseBody.append(px, lenx);
      }
      break;
    }

    case LWS_CALLBACK_COMPLETED_CLIENT_HTTP: {
      if (!ctx) break;

      ctx->responseComplete = true;

      // Create response
      auto response = std::make_unique<SignalingHttpResponse>();
      response->statusCode = ctx->statusCode;
      response->body = ctx->responseBody;
      response->headers = ctx->responseHeaders;

      if (ctx->callback) {
        ctx->callback(std::move(response));
      }

      return -1;  // Close the connection
    }

    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR: {
      if (!ctx) break;

      std::string error = "Connection error";
      if (in) {
        error = std::string(static_cast<const char*>(in), len);
      }

      PLOG_ERROR << "HTTP client connection error: " << error;

      auto response = std::make_unique<SignalingHttpResponse>();
      response->statusCode = 0;
      response->body = error;

      if (ctx->callback) {
        ctx->callback(std::move(response));
      }

      return -1;
    }

    case LWS_CALLBACK_CLOSED_CLIENT_HTTP: {
      // Connection closed - handled by COMPLETED_CLIENT_HTTP or
      // CLIENT_CONNECTION_ERROR
      break;
    }

    default:
      break;
  }

  return 0;
}

}  // namespace webrtc
}  // namespace nabto
