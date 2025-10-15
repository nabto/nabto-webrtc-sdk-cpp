#pragma once

#include <nabto/webrtc/device.hpp>
#include <libwebsockets.h>

#include "lws_context_manager.hpp"

#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <map>

namespace nabto {
namespace webrtc {

class LwsHttpClient;
using LwsHttpClientPtr = std::shared_ptr<LwsHttpClient>;

/**
 * Libwebsockets-based HTTP client implementing the SignalingHttpClient interface.
 */
class LwsHttpClient : public SignalingHttpClient,
                      public std::enable_shared_from_this<LwsHttpClient> {
 public:
  /**
   * Create an instance of the SignalingHttpClient.
   *
   * @return Smart pointer to the created SignalingHttpClient.
   */
  static SignalingHttpClientPtr create();

  LwsHttpClient();
  ~LwsHttpClient() override;
  LwsHttpClient(const LwsHttpClient&) = delete;
  LwsHttpClient& operator=(const LwsHttpClient&) = delete;
  LwsHttpClient(LwsHttpClient&&) = delete;
  LwsHttpClient& operator=(LwsHttpClient&&) = delete;

  bool sendRequest(const SignalingHttpRequest& request,
                   HttpResponseCallback cb) override;

  void stop();

 private:
  struct RequestContext {
    std::string url;
    std::string method;
    std::string host;
    std::string path;
    int port;
    bool useTls;
    std::vector<std::pair<std::string, std::string>> headers;
    std::string body;
    HttpResponseCallback callback;

    // Response data
    std::string responseBody;
    std::vector<std::pair<std::string, std::string>> responseHeaders;
    int statusCode;

    // Libwebsockets specific
    struct lws* wsi;
    bool headersSent;
    size_t bodyPos;
    bool responseComplete;
  };

  bool init();
  void serviceLoop();
  bool parseUrl(const std::string& url, std::string& host, std::string& path,
                int& port, bool& useTls);

 public:
  static int lwsCallback(struct lws* wsi, enum lws_callback_reasons reason,
                         void* user, void* in, size_t len);

 private:
  LwsContextManagerPtr contextManager_;
  std::mutex mutex_;
  bool running_;
  std::map<struct lws*, std::shared_ptr<RequestContext>> requests_;
};

}  // namespace webrtc
}  // namespace nabto
