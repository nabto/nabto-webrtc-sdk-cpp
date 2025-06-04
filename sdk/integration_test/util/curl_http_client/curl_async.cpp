#include "curl_async.hpp"

#include <iostream>

namespace nabto {
namespace example {

CurlHttpClient::CurlHttpClient() : curl_(CurlAsync::create()) {}

bool CurlHttpClient::sendRequest(
    const std::string& method, const std::string& url,
    const nabto::signaling::SignalingHttpRequest& request,
    nabto::signaling::HttpResponseCallback cb) {
  readBuffer_ = request.body;
  writeBuffer_.clear();

  CURLcode res = CURLE_OK;
  CURL* curl = curl_->getCurl();

  if ((res = curl_easy_setopt(curl, CURLOPT_URL, url.c_str())) != CURLE_OK ||
      (res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeFunc)) !=
          CURLE_OK ||
      (res = curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&writeBuffer_)) !=
          CURLE_OK ||
      (res = curl_easy_setopt(curl, CURLOPT_READFUNCTION, readFunc)) !=
          CURLE_OK ||
      (res = curl_easy_setopt(curl, CURLOPT_READDATA, (void*)&readBuffer_)) !=
          CURLE_OK) {
    std::cout << "Failed to initialize Curl request with CURLE: "
              << curl_easy_strerror(res) << std::endl;
    return false;
  }

  if (method == "POST" &&
      (res = curl_easy_setopt(curl, CURLOPT_POST, 1L)) != CURLE_OK) {
    std::cout << "Failed to initialize Curl request with CURLE: "
              << curl_easy_strerror(res) << std::endl;
    return false;
  }

  if (curlReqHeaders_ != NULL) {
    curl_slist_free_all(curlReqHeaders_);
    curlReqHeaders_ = NULL;
  }

  for (auto h : request.headers) {
    std::string combined = h.first + ": " + h.second;
    curlReqHeaders_ = curl_slist_append(curlReqHeaders_, combined.c_str());
  }

  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, curlReqHeaders_);

  auto self = shared_from_this();
  curl_->asyncInvoke([self, cb, curl](CURLcode res, uint16_t statusCode) {
    // std::cout << "Got curl response code: " << res << " statusCode: " <<
    // statusCode << std::endl;
    if (res != CURLE_OK) {
      std::cout << "Failed to make attach request" << std::endl;
      std::string err = "Request failed";
      cb(nullptr);
      return;
    }
    // std::cout << "Response data: " << self->writeBuffer_ << std::endl;
    auto response = std::make_unique<nabto::signaling::SignalingHttpResponse>();
    response->body = self->writeBuffer_;
    response->statusCode = statusCode;

    // TODO read the headers
    cb(std::move(response));
  });
  return true;
}

size_t CurlHttpClient::readFunc(void* ptr, size_t size, size_t nmemb, void* s) {
  try {
    std::string* buf = (std::string*)s;
    size_t ncpy = size * nmemb;
    if (size * nmemb > buf->size()) {
      ncpy = buf->size();
    }
    // std::cout << "size: " << size << " nmemb: " << nmemb << " buf size: " <<
    // buf->size(); std::cout << "Copying " << ncpy << "bytes from: " << buf;
    memcpy(ptr, buf->data(), ncpy);
    *buf = buf->substr(ncpy);
    return ncpy;
  } catch (std::exception& ex) {
    std::cout << "readFunc failure" << std::endl;
    return 0;
  }
}

size_t CurlHttpClient::writeFunc(void* ptr, size_t size, size_t nmemb,
                                 void* s) {
  if (s == stdout) {
    // std::cout << "s was stdout, this is header data";
    std::string data((char*)ptr, size * nmemb);
    std::cout << data << std::endl;
    return size * nmemb;
  }
  try {
    ((std::string*)s)->append((char*)ptr, size * nmemb);
  } catch (std::exception& ex) {
    std::cout << "WriteFunc failure" << std::endl;
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

CurlAsync::CurlAsync() {}

CurlAsync::~CurlAsync() {
  try {
    thread_ = std::thread();
  } catch (std::exception& ex) {
    std::cout << "~CurlAsync exception " << ex.what() << std::endl;
  }
  curl_easy_cleanup(curl_);
  curl_global_cleanup();
}

bool CurlAsync::init() {
  CURLcode res;
  res = curl_global_init(CURL_GLOBAL_ALL);
  if (res != CURLE_OK) {
    std::cout << "Failed to initialize Curl global with: "
              << curl_easy_strerror(res) << std::endl;
    return false;
  }
  curl_ = curl_easy_init();
  if (!curl_) {
    std::cout << "Failed to initialize Curl easy with: "
              << curl_easy_strerror(res) << std::endl;
    return false;
  }

  res = curl_easy_setopt(curl_, CURLOPT_VERBOSE, 0L);
  if (res != CURLE_OK) {
    std::cout << "Failed to set curl logging option with: "
              << curl_easy_strerror(res) << std::endl;
    return false;
  }
  res = curl_easy_setopt(curl_, CURLOPT_NOPROGRESS, 1L);
  if (res != CURLE_OK) {
    std::cout << "Failed to set Curl progress option with: "
              << curl_easy_strerror(res) << std::endl;
    return false;
  }
  return true;
}

void CurlAsync::stop() {
  std::cout << "CurlAsync stopped" << std::endl;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    stopped_ = true;
  }
  if (thread_.joinable()) {
    thread_.join();
  }
  std::cout << "CurlAsync stop joined" << std::endl;
}

bool CurlAsync::asyncInvoke(
    std::function<void(CURLcode res, uint16_t statusCode)> callback) {
  // std::cout << "CurlAsync asyncInvoke" << std::endl;
  bool shouldReinvoke = false;
  {
    std::lock_guard<std::mutex> lock(mutex_);
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
    std::function<void(CURLcode res, uint16_t statusCode)> callback) {
  // std::cout << "CurlAsync asyncReinvoke" << std::endl;
  std::lock_guard<std::mutex> lock(mutex_);
  callback_ = callback;
  reinvoke_ = true;
}

CURLcode CurlAsync::reinvoke() { return curl_easy_perform(curl_); }

void CurlAsync::reinvokeStatus(CURLcode* code, uint16_t* status) {
  CURLcode res = curl_easy_perform(curl_);
  *code = res;
  if (res == CURLE_OK) {
    curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, status);
  }
}

void CurlAsync::threadRunner(CurlAsync* self) {
  CURLcode res = CURLE_OK;
  while (true) {
    std::function<void(CURLcode res, uint16_t statusCode)> cb;
    long statusCode = 0;
    {
      std::lock_guard<std::mutex> lock(self->mutex_);
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
  std::lock_guard<std::mutex> lock(self->mutex_);
  if (!self->stopped_) {
    self->thread_.detach();
  }
  me = self->me_;
  self->me_ = nullptr;
}

}  // namespace example
}  // namespace nabto
