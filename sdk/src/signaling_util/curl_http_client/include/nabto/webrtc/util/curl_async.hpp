#pragma once

#include <curl/curl.h>

#include <nabto/webrtc/device.hpp>

#include <functional>
#include <memory>
#include <mutex>
#include <thread>

namespace nabto {
namespace util {

class CurlAsync;
using CurlAsyncPtr = std::shared_ptr<CurlAsync>;

/**
 * Curl based HTTP client implementing the SignalingHttpClient interface used by
 * the SDK.
 */
class CurlHttpClient : public nabto::signaling::SignalingHttpClient,
                       public std::enable_shared_from_this<CurlHttpClient> {
 public:
  /**
   * Create an instance of the SignalingHttpClient.
   *
   * @return Smart pointer to the created SignalingHttpClient.
   */
  static nabto::signaling::SignalingHttpClientPtr create();

  CurlHttpClient();
  bool sendRequest(const nabto::signaling::SignalingHttpRequest& request,
                   nabto::signaling::HttpResponseCallback cb) override;

 private:
  CurlAsyncPtr curl_ = nullptr;
  std::string readBuffer_;
  std::string writeBuffer_;
  std::string authHeader_;
  std::string ctHeader_;

  struct curl_slist* curlReqHeaders_ = nullptr;

  static size_t readFunc(void* ptr, size_t size, size_t nmemb, void* s);
  static size_t writeFunc(void* ptr, size_t size, size_t nmemb, void* s);
};

/**
 * Callback invoked when a curl request has been resolved.
 *
 * @param res The resulting Curl code.
 * @param statusCode Iff the request was successful, the status code from the
 * HTTP response, 0 otherwise.
 */
using CurlAsyncInvokeCallback =
    std::function<void(CURLcode res, uint16_t statusCode)>;

/**
 * Wrapper class to turn Curl easy into an async HTTP client
 */
class CurlAsync : public std::enable_shared_from_this<CurlAsync> {
 public:
  /**
   * Create an CurlAsync instance.
   *
   * @return Smart pointer to the created object.
   */
  static CurlAsyncPtr create();

  CurlAsync() = default;
  ~CurlAsync();
  CurlAsync(const CurlAsync&) = delete;
  CurlAsync& operator=(const CurlAsync&) = delete;
  CurlAsync(CurlAsync&&) = delete;
  CurlAsync& operator=(CurlAsync&&) = delete;

  // Init method called by the create function
  bool init();

  /**
   * Stop the client and join the worker thread.
   */
  void stop();

  /**
   * Get the underlying Curl context to build your request.
   *
   * @return Pointer to the Curl context.
   */
  CURL* getCurl();

  /**
   * Invokes the request. This starts a new thread in which the request is
   * invoked synchronously. The callback is invoked from the created thread once
   * the request is resolved.
   *
   * @param callback The callback to be invoked when the request is resolved.
   * @return false if a thread is already running. True if the thread was
   * started.
   */
  bool asyncInvoke(const CurlAsyncInvokeCallback& callback);

  /**
   * Reinvoke the request. This must be called from the callback of a previous
   * request. This request will reuse the std::thread created for the first
   * request. getCurl() can be used to build a new request in the first callback
   * before calling this.
   *
   * @param callback The callback to invoke when the request is resolved.
   */
  void asyncReinvoke(const CurlAsyncInvokeCallback& callback);

  /**
   * Reinvoke the request. This must be called from the callback of a previous
   * request and is a direct blocking invocation of the curl_easy_perform().
   *
   * @return Curl code returned by curl_easy_perform()
   */
  CURLcode reinvoke();

  /**
   * Reinvoke the request and if successful return the status code of the HTTP
   * response.
   *
   * This must be called from the callback of a previous
   * request and is a direct blocking invocation of the curl_easy_perform().
   *
   * @param code Where to store the curl code returned by curl_easy_perform()
   * @param status Where to store the HTTP response status if a response was
   * received successfully.
   */
  void reinvokeStatus(CURLcode* code, uint16_t* status);

 private:
  static void threadRunner(CurlAsync* self);

  CURL* curl_ = nullptr;
  std::thread thread_;
  std::mutex mutex_;
  std::function<void(CURLcode res, uint16_t statusCode)> callback_;
  bool reinvoke_ = false;
  bool stopped_ = false;
  CurlAsyncPtr me_ = nullptr;
};

}  // namespace util
}  // namespace nabto
