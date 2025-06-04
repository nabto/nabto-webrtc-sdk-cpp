#pragma once

// Required. See: https://github.com/microsoft/cpprestsdk/issues/230
#define _TURN_OFF_PLATFORM_STRING

#include <CppRestOpenAPIClient/api/DefaultApi.h>
#include <nabto/webrtc/util/curl_async.hpp>
#include "util/libdatachannel_websocket/rtc_websocket_wrapper.hpp"
#include <plog/Appenders/ColorConsoleAppender.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Init.h>
#include <nabto/webrtc/util/std_timer.hpp>

#include <nabto/webrtc/device.hpp>

#include <iostream>
#include <memory>

namespace nabto {
namespace test {

using namespace org::openapitools::client;

class DeviceWithClient {
 public:
  std::string id;
  nabto::signaling::SignalingDevicePtr device;
  nabto::signaling::SignalingChannelPtr channel;
  bool authorized;
};

class ProtocolError {
 public:
  std::string code;
  std::string message;
};

class TestTokenGen : public nabto::signaling::SignalingTokenGenerator,
                     public std::enable_shared_from_this<TestTokenGen> {
 public:
  std::string token_;
  static nabto::signaling::SignalingTokenGeneratorPtr create(
      std::string token) {
    return std::make_shared<TestTokenGen>(token);
  }
  TestTokenGen(std::string& token) : token_(token) {}

  bool generateToken(std::string& token) override {
    token = token_;
    return true;
  }
};

class TestInstance : public std::enable_shared_from_this<TestInstance> {
 public:
  std::string productId_;
  std::string deviceId_;
  std::string epUrl_;
  std::string testId_;
  std::string accessToken_;

  static std::shared_ptr<TestInstance> create(bool extraRespData, bool failHttp,
                                              bool failWs) {
    return std::make_shared<TestInstance>(extraRespData, failHttp, failWs);
  }
  static std::shared_ptr<TestInstance> create() {
    auto ti = std::make_shared<TestInstance>(false, false, false);
    return ti;
  }

  TestInstance(bool extraRespData, bool failHttp, bool failWs) {
    auto apiConf = std::make_shared<api::ApiConfiguration>();
    apiConf->setBaseUrl("http://127.0.0.1:13745");
    auto apiCli = std::make_shared<api::ApiClient>(apiConf);
    api_ = std::make_shared<api::DefaultApi>(apiCli);

    auto req = std::make_shared<model::PostTestDevice_request>();
    req->setFailHttp(failHttp);
    req->setFailWs(failWs);
    req->setExtraDeviceConnectResponseData(extraRespData);

    auto task = api_->postTestDevice(req);
    auto resp = task.get();
    if (resp) {
      productId_ = resp->getProductId();
      deviceId_ = resp->getDeviceId();
      epUrl_ = resp->getEndpointUrl();
      testId_ = resp->getTestId();
      accessToken_ = resp->getAccessToken();
    } else {
      std::cout << "RESP FAILED" << resp << std::endl;
    }
  }

  ~TestInstance() {
    if (api_) {
      auto task = api_->deleteTestDeviceByTestId(testId_);
      auto resp = task.get();
    }
  }

  nabto::signaling::SignalingDevicePtr createDevice() {
    http_ = nabto::example::CurlHttpClient::create();
    ws_ = nabto::example::RtcWebsocketWrapper::create();
    tf_ = nabto::example::StdTimerFactory::create();
    tokGen_ = TestTokenGen::create(accessToken_);
    auto self = shared_from_this();

    conf_ = {deviceId_, productId_, tokGen_, epUrl_, ws_, http_, tf_};

    sig_ = nabto::signaling::SignalingDeviceFactory::create(conf_);
    return sig_;
  }

  nabto::signaling::SignalingDevicePtr createConnectedDevice() {
    auto dev = createDevice();
    std::promise<void> connProm;
    dev->setStateChangeHandler(
        [&connProm](nabto::signaling::SignalingDeviceState state) {
          if (state == nabto::signaling::SignalingDeviceState::CONNECTED) {
            connProm.set_value();
          }
        });
    dev->start();
    std::future<void> f = connProm.get_future();
    f.get();
    return dev;
  }

  std::string createClient() {
    try {
      auto resp = api_->postTestDeviceByTestIdClients(testId_).get();
      std::string cliId = resp->getClientId();
      return cliId;
    } catch (api::ApiException& ex) {
      std::cout << "Failed to create client: " << ex.what() << std::endl;
      throw std::runtime_error("Failed");
    }
  }

  bool connectClient(std::string cliId) {
    try {
      auto resp =
          api_->postTestDeviceByTestIdClientsByClientIdConnect(testId_, cliId)
              .get();
      return true;
    } catch (api::ApiException& ex) {
      std::cout << "Failed to connect client: " << ex.what() << std::endl;
      throw std::runtime_error("HTTP Failed");
    }
  }

  DeviceWithClient createDeviceWithConnectedCli() {
    DeviceWithClient res;
    res.device = createConnectedDevice();
    res.id = createClient();
    std::promise<void> prom;

    res.device->setNewChannelHandler(
        [&res, &prom](nabto::signaling::SignalingChannelPtr conn,
                      bool authorized) {
          res.channel = conn;
          res.authorized = authorized;
          prom.set_value();
        });
    connectClient(res.id);
    clientSendMessage(res.id, "hello");
    auto f = prom.get_future();
    f.get();
    return res;
  }

  bool clientSendMessage(std::string& cliId, std::string message) {
    std::vector<std::string> msgs = {message};
    auto req = std::make_shared<
        model::PostTestClientByTestIdSend_device_messages_request>();
    req->setMessages(msgs);

    try {
      auto resp = api_->postTestDeviceByTestIdClientsByClientIdSendMessages(
                          testId_, cliId, req)
                      .get();
      return true;
    } catch (api::ApiException& ex) {
      std::cout << "Failed to send messages: " << ex.what() << std::endl;
      throw std::runtime_error("HTTP Failed");
    }
  }

  bool disconnectClient(std::string& cliId) {
    try {
      auto resp = api_->postTestDeviceByTestIdClientsByClientIdDisconnect(
                          testId_, cliId)
                      .get();
      return true;
    } catch (api::ApiException& ex) {
      std::cout << "Failed to disconnect: " << ex.what() << std::endl;
      throw std::runtime_error("HTTP Failed");
    }
  }

  bool sendNewMessageType() {
    try {
      auto resp = api_->postTestDeviceByTestIdSendNewMessageType(testId_).get();
      return true;
    } catch (api::ApiException& ex) {
      std::cout << "Failed to send new message type: " << ex.what()
                << std::endl;
      // if (resp) {
      //     auto cBody = object_convertToJSON(resp);
      //     std::string err = std::string("HTTP Failed: ") +
      //     cJSON_Print(cBody); cJSON_Delete(cBody); throw
      //     std::runtime_error(err);
      // }
      throw std::runtime_error("HTTP Failed");
    }
  }

  std::vector<std::string> clientWaitForMessagesIsReceived(
      std::string cliId, std::vector<std::string>& msgs) {
    auto req = std::make_shared<
        model::PostTestClientByTestIdWait_for_device_messages_request>();
    req->setTimeout(200);
    req->setMessages(msgs);

    try {
      auto resp = api_->postTestDeviceByTestIdClientsByClientIdWaitForMessages(
                          testId_, cliId, req)
                      .get();
      return resp->getMessages();
    } catch (api::ApiException& ex) {
      std::cout << "Failed to wait for messages: " << ex.what() << std::endl;
      throw std::runtime_error("HTTP Failed");
    }
  }

  ProtocolError clientWaitForErrorReceived(std::string cliId) {
    auto req = std::make_shared<
        model::PostTestDeviceByTestIdClientsByClientIdWait_for_error_request>();
    req->setTimeout(200);

    try {
      auto resp = api_->postTestDeviceByTestIdClientsByClientIdWaitForError(
                          testId_, cliId, req)
                      .get();
      ProtocolError ret = {resp->getErrorCode(), resp->getErrorMessage()};
      return ret;
    } catch (api::ApiException& ex) {
      std::cout << "Failed to wait for error: " << ex.what() << std::endl;
      throw std::runtime_error("HTTP Failed");
    }
  }

  bool clientSendError(std::string cliId, std::string errorCode,
                       std::string errorMessage) {
    auto req = std::make_shared<
        model::PostTestClientByTestIdSend_device_error_request>();
    req->setErrorCode(errorCode);
    req->setErrorMessage(errorMessage);

    try {
      auto resp = api_->postTestDeviceByTestIdClientsByClientIdSendError(
                          testId_, cliId, req)
                      .get();
      return true;
    } catch (api::ApiException& ex) {
      std::cout << "Failed to send new message type: " << ex.what()
                << std::endl;
      throw std::runtime_error("HTTP Failed");
    }
  }

  bool disconnectDevice() {
    try {
      auto resp = api_->postTestDeviceByTestIdDisconnectDevice(testId_).get();
      return true;
    } catch (api::ApiException& ex) {
      std::cout << "Failed to disconnect device: " << ex.what() << std::endl;
      throw std::runtime_error("HTTP Failed");
    }
  }

  nabto::signaling::SignalingDevicePtr getDevice() { return sig_; }

 private:
  std::shared_ptr<api::DefaultApi> api_ = nullptr;
  nabto::signaling::SignalingHttpClientPtr http_;
  nabto::signaling::SignalingWebsocketPtr ws_;
  nabto::signaling::SignalingTimerFactoryPtr tf_;
  nabto::signaling::SignalingDeviceConfig conf_;
  nabto::signaling::SignalingDevicePtr sig_;
  nabto::signaling::SignalingTokenGeneratorPtr tokGen_;
};

}  // namespace test
}  // namespace nabto
