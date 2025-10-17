#pragma once

#include <mutex>
#include <unordered_map>

#include <nabto/webrtc/device.hpp>
#include "lws_context_manager.hpp"

namespace nabto::example {

class LwsHttpClient : public nabto::webrtc::SignalingHttpClient {
 public:
  static nabto::webrtc::SignalingHttpClientPtr create() {
    return std::make_shared<LwsHttpClient>();
  }

  static int lwsCallback(struct lws* wsi, enum lws_callback_reasons reason,
                         void* user, void* in, size_t len);

  LwsHttpClient();
  ~LwsHttpClient() override;
  LwsHttpClient(const LwsHttpClient&) = delete;
  LwsHttpClient& operator=(const LwsHttpClient&) = delete;
  LwsHttpClient(LwsHttpClient&&) = delete;
  LwsHttpClient& operator=(LwsHttpClient&&) = delete;

  bool sendRequest(const nabto::webrtc::SignalingHttpRequest& request,
                   nabto::webrtc::HttpResponseCallback cb) override;

 private:
  struct Request {
    LwsHttpClient* client;
    nabto::webrtc::HttpResponseCallback cb;
    nabto::webrtc::SignalingHttpRequest req;
    std::string body;
    int statusCode;
    size_t writtenBytes;

    uint64_t handle;
    struct lws* wsi;
  };
  uint64_t requestIndex_ = 0;
  static std::unordered_map<uint64_t, Request> requests_;

  void cleanup();

  std::shared_ptr<LwsContextManager> contextManager_ = LwsContextManager::getInstance();
  std::atomic<bool> connected_{false};
  

  std::mutex sendMutex_;
};

} // nabto::example
