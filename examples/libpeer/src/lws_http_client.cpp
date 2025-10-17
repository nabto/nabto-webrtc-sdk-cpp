#include <iostream>
#include <nabto/webrtc/util/logging.hpp>
#include "lws_http_client.hpp"
#include "util.hpp"

namespace nabto::example {

std::unordered_map<uint64_t, LwsHttpClient::Request> LwsHttpClient::requests_;

LwsHttpClient::LwsHttpClient() {
}

LwsHttpClient::~LwsHttpClient() {
  cleanup();
}

void LwsHttpClient::cleanup() {
}

bool LwsHttpClient::sendRequest(const nabto::webrtc::SignalingHttpRequest& request,
                                nabto::webrtc::HttpResponseCallback cb) {
  std::lock_guard<std::mutex> lock(sendMutex_);

  auto parsed = parseUrl(request.url);
  auto protocol = std::get<0>(parsed);
  auto host = std::get<1>(parsed);
  auto path = std::get<2>(parsed);

  struct lws_client_connect_info ccinfo = {};
  ccinfo.context = contextManager_->getContext();

  ccinfo.ssl_connection = LCCSCF_USE_SSL;
  ccinfo.port = 443;
  ccinfo.address = host.c_str();
  ccinfo.path = path.c_str();
  ccinfo.host = ccinfo.address;
  ccinfo.origin = ccinfo.address;
  ccinfo.method = request.method.c_str();
  ccinfo.alpn = "http/1.1";

  NPLOGI << request.method << "  " << request.body;

  uint64_t handle = requestIndex_++;
  requests_[handle] = { this, cb, request, "", 0, 0 };
  requests_[handle].req.headers.push_back({ "Content-Length", std::to_string(request.body.length()) });

  ccinfo.protocol = "lws-http-protocol";
  ccinfo.userdata = reinterpret_cast<void*>(handle);
  ccinfo.userdata = (void*)handle;
  ccinfo.pwsi = &requests_[handle].wsi;

  if (!lws_client_connect_via_info(&ccinfo)) {
    NPLOGE << "Client creation failed!";
    return false;
  }

  return true;
}

int LwsHttpClient::lwsCallback(struct lws* wsi, enum lws_callback_reasons reason,
                               void* user, void* in, size_t len) {
  uint64_t handle = (uint64_t)user;
  Request& r = requests_[handle];

  char buf[LWS_PRE + 1024];
  char* start = &buf[LWS_PRE];
  char*  end = &buf[sizeof(buf) - 1];
  char* p = start;
  int n;

  switch (reason) {
    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR: {
      NPLOGI << "CLIENT_CONNECTION_ERROR";
      break;
    }

    case LWS_CALLBACK_CLOSED_CLIENT_HTTP: {
      NPLOGI << "CLOSED_CLIENT_HTTP";
      requests_.erase(handle);
      break;
    }

    // Receiving callbacks

    case LWS_CALLBACK_ESTABLISHED_CLIENT_HTTP: {
      int status = lws_http_client_http_response(wsi);
      NPLOGI << "ESTABLISHED_CLIENT_HTTP : " << status;
      r.statusCode = status;
      break;
    }

    case LWS_CALLBACK_RECEIVE_CLIENT_HTTP_READ: {
      NPLOGI << "RECEIVE_CLIENT_HTTP_READ : read " << len;
      std::string chunk(static_cast<const char*>(in), len);
      r.body += chunk;
      break;
    }

    case LWS_CALLBACK_RECEIVE_CLIENT_HTTP: {
      n = sizeof(buf) - LWS_PRE;
      if (lws_http_client_read(wsi, &p, &n) < 0) {
        return -1;
      }
      return 0;
    }

    case LWS_CALLBACK_COMPLETED_CLIENT_HTTP: {
      NPLOGI << "COMPLETED_CLIENT_HTTP : status " << r.statusCode << " : " << r.body;
      auto response = std::make_unique<nabto::webrtc::SignalingHttpResponse>();
      response->statusCode = r.statusCode;
      response->body = r.body;
      r.cb(std::move(response));
      break;
    }

    // Callbacks for generating POST data

    case LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER: {
      if (!lws_http_is_redirected_to_get(wsi)) {
        NPLOGI << "APPEND_HANDSHAKE_HEADER : doing POST flow";

        unsigned char **headerPtr = (unsigned char**)in;
        unsigned char *headerEnd = (*headerPtr) + len;

        for (auto& element : r.req.headers) {
          lws_add_http_header_by_name(
            wsi,
            (unsigned char*)element.first.c_str(),
            (unsigned char*)element.second.c_str(),
            element.second.length(),
            headerPtr,
            headerEnd
          );
        }

        lws_client_http_body_pending(wsi, 1);
        lws_callback_on_writable(wsi);
      } else {
        NPLOGI << "APPEND_HANDSHAKE_HEADER : doing GET flow";
      }

      return 0;
    }

    case LWS_CALLBACK_CLIENT_HTTP_WRITEABLE: {
      NPLOGI << "CLIENT_HTTP_WRITEABLE";

      if (lws_http_is_redirected_to_get(wsi)) {
        break;
      }

      enum lws_write_protocol prot = LWS_WRITE_HTTP;
      size_t remaining = r.req.body.length() - r.writtenBytes;
      size_t chunkSize = std::min(remaining, (size_t)1024);

      memcpy(start, r.req.body.c_str() + r.writtenBytes, chunkSize);
      r.writtenBytes += chunkSize;

      if (r.writtenBytes >= r.req.body.length()) {
        prot = LWS_WRITE_HTTP_FINAL;
      }

      int written = lws_write(wsi, (uint8_t*)start, chunkSize, prot);

      if (written != chunkSize) {
        return 1;
      }

      if (prot != LWS_WRITE_HTTP_FINAL) {
        lws_callback_on_writable(wsi);
      } else {
        lws_client_http_body_pending(wsi, 0);
      }

      return 0;
    }

    default: {
      break;
    }
  }

  return lws_callback_http_dummy(wsi, reason, user, in, len);
}

} // nabto::webrtc
