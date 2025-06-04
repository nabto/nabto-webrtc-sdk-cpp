#pragma once

#include <curl/curl.h>

#include <nabto/webrtc/device.hpp>

#include <functional>
#include <memory>
#include <mutex>
#include <thread>

namespace nabto {
namespace example {

class CurlAsync;
typedef std::shared_ptr<CurlAsync> CurlAsyncPtr;

class CurlHttpClient : public nabto::signaling::SignalingHttpClient,
                       public std::enable_shared_from_this<CurlHttpClient> {
 public:
  static nabto::signaling::SignalingHttpClientPtr create() {
    return std::make_shared<CurlHttpClient>();
  }
  CurlHttpClient();
  bool sendRequest(const std::string& method, const std::string& url,
                   const nabto::signaling::SignalingHttpRequest& request,
                   nabto::signaling::HttpResponseCallback cb);

 private:
  CurlAsyncPtr curl_ = nullptr;
  std::string readBuffer_;
  std::string writeBuffer_;
  std::string authHeader_;
  std::string ctHeader_;

  struct curl_slist* curlReqHeaders_ = NULL;

  static size_t readFunc(void* ptr, size_t size, size_t nmemb, void* s);
  static size_t writeFunc(void* ptr, size_t size, size_t nmemb, void* s);
};

class CurlAsync : public std::enable_shared_from_this<CurlAsync> {
 public:
  static CurlAsyncPtr create();
  CurlAsync();
  ~CurlAsync();

  bool init();
  void stop();

  // Get the curl reference to build your request on using `curl_easy_XX()`
  // functions
  CURL* getCurl() { return curl_; }

  // Invokes the request. This starts a new thread in which the request is
  // invoked synchroneusly. The callback is invoked from the created thread once
  // the request is resolved. Returns false if a thread is already running
  bool asyncInvoke(
      std::function<void(CURLcode res, uint16_t statusCode)> callback);

  // Reinvoke the request. This must be called from the callback of a previous
  // request. This request will reuse the std::thread created for the first
  // request. getCurl() can be used to build a new request in the first callback
  // before calling this.
  void asyncReinvoke(
      std::function<void(CURLcode res, uint16_t statusCode)> callback);

  // Reinvoke the request. This must be called from the callback of a previous
  // request and is a direct blocking invocation of the curl_easy_perform.
  CURLcode reinvoke();
  void reinvokeStatus(CURLcode* code, uint16_t* status);

 private:
  static void threadRunner(CurlAsync* self);

  CURL* curl_;
  std::thread thread_;
  std::mutex mutex_;
  std::function<void(CURLcode res, uint16_t statusCode)> callback_;
  bool reinvoke_ = false;
  bool stopped_ = false;
  CurlAsyncPtr me_ = nullptr;
};

}  // namespace example
}  // namespace nabto
