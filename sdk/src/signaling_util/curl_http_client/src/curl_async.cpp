#include <curl/curl.h>
#include <curl/easy.h>

#include <nabto/webrtc/device.hpp>
#include <nabto/webrtc/util/curl_async.hpp>
#include <nabto/webrtc/util/logging.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <utility>

namespace nabto {
namespace util {

nabto::signaling::SignalingHttpClientPtr CurlHttpClient::create() {
  return std::make_shared<CurlHttpClient>();
}

CurlHttpClient::CurlHttpClient() : curl_(CurlAsync::create()) {}

CurlHttpClient::~CurlHttpClient() {
  if (curlReqHeaders_ != nullptr) {
    curl_slist_free_all(curlReqHeaders_);
    curlReqHeaders_ = nullptr;
  }
}

bool CurlHttpClient::sendRequest(
    const nabto::signaling::SignalingHttpRequest& request,
    nabto::signaling::HttpResponseCallback cb) {
  readBuffer_ = request.body;
  writeBuffer_.clear();

  CURLcode res = CURLE_OK;
  CURL* curl = curl_->getCurl();

  NPLOGD << "Sending HTTP request";

  res = curl_easy_setopt(curl, CURLOPT_URL, request.url.c_str());
  if (res != CURLE_OK) {
    NPLOGE << "Failed to initialize Curl request with CURLE: "
           << curl_easy_strerror(res);
    return false;
  }

  res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeFunc);
  if (res != CURLE_OK) {
    NPLOGE << "Failed to initialize Curl request with CURLE: "
           << curl_easy_strerror(res);
    return false;
  }

  res = curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&writeBuffer_);
  if (res != CURLE_OK) {
    NPLOGE << "Failed to initialize Curl request with CURLE: "
           << curl_easy_strerror(res);
    return false;
  }

  res = curl_easy_setopt(curl, CURLOPT_READFUNCTION, readFunc);

  if (res != CURLE_OK) {
    NPLOGE << "Failed to initialize Curl request with CURLE: "
           << curl_easy_strerror(res);
    return false;
  }

  res = curl_easy_setopt(curl, CURLOPT_READDATA, (void*)&readBuffer_);

  if (res != CURLE_OK) {
    NPLOGE << "Failed to initialize Curl request with CURLE: "
           << curl_easy_strerror(res);
    return false;
  }

  if (request.method == "POST") {
    res = curl_easy_setopt(curl, CURLOPT_POST, 1L);
    if (res != CURLE_OK) {
      NPLOGE << "Failed to initialize Curl request with CURLE: "
             << curl_easy_strerror(res);
      return false;
    }
  }

  if (curlReqHeaders_ != nullptr) {
    curl_slist_free_all(curlReqHeaders_);
    curlReqHeaders_ = nullptr;
  }

  for (const auto& h : request.headers) {
    const std::string combined = h.first + ": " + h.second;
    curlReqHeaders_ = curl_slist_append(curlReqHeaders_, combined.c_str());
  }

  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, curlReqHeaders_);

  auto self = shared_from_this();
  curl_->asyncInvoke([self, cb, curl](CURLcode res, uint16_t statusCode) {
    NPLOGI << "Got curl response code: " << res
           << " statusCode: " << statusCode;
    if (res != CURLE_OK) {
      NPLOGE << "Failed to make attach request";
      cb(nullptr);
      return;
    }
    NPLOGI << "Response data: " << self->writeBuffer_;
    auto response = std::make_unique<nabto::signaling::SignalingHttpResponse>();
    response->body = self->writeBuffer_;
    response->statusCode = statusCode;

    // TODO(tk) read the headers
    cb(std::move(response));
  });
  return true;
}

size_t CurlHttpClient::readFunc(void* ptr, size_t size, size_t nmemb, void* s) {
  try {
    auto* buf = static_cast<std::string*>(s);
    size_t ncpy = size * nmemb;
    if (size * nmemb > buf->size()) {
      ncpy = buf->size();
    }
    // NPLOGI << "size: " << size << " nmemb: " << nmemb << " buf size: " <<
    // buf->size(); NPLOGI << "Copying " << ncpy << "bytes from: " << buf;
    memcpy(ptr, buf->data(), ncpy);
    *buf = buf->substr(ncpy);
    return ncpy;
  } catch (std::exception& ex) {
    NPLOGE << "readFunc failure";
    return 0;
  }
}

size_t CurlHttpClient::writeFunc(void* ptr, size_t size, size_t nmemb,
                                 void* s) {
  if (s == stdout) {
    // NPLOGI << "s was stdout, this is header data";
    const std::string data(static_cast<char*>(ptr), size * nmemb);
    NPLOGI << data;
    return size * nmemb;
  }
  try {
    auto* str = static_cast<std::string*>(s);
    str->append(static_cast<char*>(ptr), size * nmemb);
  } catch (std::exception& ex) {
    NPLOGE << "WriteFunc failure";
    return size * nmemb;
  }
  return size * nmemb;
}

CurlAsyncPtr CurlAsync::create() {
  auto c = std::make_shared<CurlAsync>();
  if (c->init()) {
    return c;
  }
  return nullptr;
}

CurlAsync::~CurlAsync() {
  if (!stopped_) {
    stop();
  }
  curl_easy_cleanup(curl_);
  curl_global_cleanup();
}

bool CurlAsync::init() {
  CURLcode res = curl_global_init(CURL_GLOBAL_ALL);
  if (res != CURLE_OK) {
    NPLOGE << "Failed to initialize Curl global with: "
           << curl_easy_strerror(res);
    return false;
  }
  curl_ = curl_easy_init();
  if (curl_ == nullptr) {
    NPLOGE << "Failed to initialize Curl easy with: "
           << curl_easy_strerror(res);
    return false;
  }

  res = curl_easy_setopt(curl_, CURLOPT_VERBOSE, 0L);
  if (res != CURLE_OK) {
    NPLOGE << "Failed to set curl logging option with: "
           << curl_easy_strerror(res);
    return false;
  }
  res = curl_easy_setopt(curl_, CURLOPT_NOPROGRESS, 1L);
  if (res != CURLE_OK) {
    NPLOGE << "Failed to set Curl progress option with: "
           << curl_easy_strerror(res);
    return false;
  }
  return true;
}

void CurlAsync::stop() {
  NPLOGD << "CurlAsync stopped";
  {
    const std::lock_guard<std::mutex> lock(mutex_);
    stopped_ = true;
  }
  if (thread_.joinable()) {
    thread_.join();
  }
  NPLOGD << "CurlAsync stop joined";
}

CURL* CurlAsync::getCurl() { return curl_; }

bool CurlAsync::asyncInvoke(
    const std::function<void(CURLcode res, uint16_t statusCode)>& callback) {
  NPLOGD << "CurlAsync asyncInvoke";
  bool shouldReinvoke = false;
  {
    const std::lock_guard<std::mutex> lock(mutex_);
    if (!(thread_.get_id() == std::thread::id())) {
      // Thread is already running
      shouldReinvoke = true;
    }
  }
  if (shouldReinvoke) {
    asyncReinvoke(callback);
    return true;
  }
  // Thread is not running, no concurrency issues
  callback_ = callback;
  reinvoke_ = true;
  me_ = shared_from_this();
  thread_ = std::thread(threadRunner, this);
  return true;
}

void CurlAsync::asyncReinvoke(
    const std::function<void(CURLcode res, uint16_t statusCode)>& callback) {
  NPLOGD << "CurlAsync asyncReinvoke";
  const std::lock_guard<std::mutex> lock(mutex_);
  callback_ = callback;
  reinvoke_ = true;
}

CURLcode CurlAsync::reinvoke() { return curl_easy_perform(curl_); }

void CurlAsync::reinvokeStatus(CURLcode* code, uint16_t* status) {
  const CURLcode res = curl_easy_perform(curl_);
  *code = res;
  if (res == CURLE_OK) {
    curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, status);
  }
}

void CurlAsync::threadRunner(CurlAsync* self) {
  CURLcode res = CURLE_OK;
  while (true) {
    std::function<void(CURLcode res, uint16_t statusCode)> cb;
    int64_t statusCode = 0;
    {
      const std::lock_guard<std::mutex> lock(self->mutex_);
      if (self->stopped_ || !self->reinvoke_) {
        break;
      }
      res = curl_easy_perform(self->curl_);
      if (res == CURLE_OK) {
        curl_easy_getinfo(self->curl_, CURLINFO_RESPONSE_CODE, &statusCode);
      }
      self->reinvoke_ = false;
      cb = self->callback_;
      self->callback_ = nullptr;
    }
    cb(res, statusCode);
  }
  CurlAsyncPtr me = nullptr;
  const std::lock_guard<std::mutex> lock(self->mutex_);
  if (!self->stopped_) {
    self->thread_.detach();
  }
  me = self->me_;
  self->me_ = nullptr;
}

}  // namespace util
}  // namespace nabto
