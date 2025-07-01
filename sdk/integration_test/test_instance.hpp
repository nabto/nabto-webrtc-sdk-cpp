#pragma once

// Required. See: https://github.com/microsoft/cpprestsdk/issues/230
// #define _TURN_OFF_PLATFORM_STRING

#include "util/libdatachannel_websocket/rtc_websocket_wrapper.hpp"
#include <CppRestOpenAPIClient/api/DefaultApi.h>
#include <cpprest/json.h>
#include <plog/Appenders/ColorConsoleAppender.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Init.h>

#include <nabto/webrtc/device.hpp>
#include <nabto/webrtc/util/curl_async.hpp>
#include <nabto/webrtc/util/std_timer.hpp>

#include <iostream>
#include <memory>

namespace nabto {
namespace test {

using namespace org::openapitools::client;

class DeviceWithClient {
 public:
  std::string id;
  nabto::webrtc::SignalingDevicePtr device;
  nabto::webrtc::SignalingChannelPtr channel;
  bool authorized;
};

class ProtocolError {
 public:
  std::string code;
  std::string message;
};

class TestTokenGen : public nabto::webrtc::SignalingTokenGenerator,
                     public std::enable_shared_from_this<TestTokenGen> {
 public:
  std::string token_;
  static nabto::webrtc::SignalingTokenGeneratorPtr create(std::string token) {
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

  nabto::webrtc::SignalingDevicePtr createDevice() {
    http_ = nabto::webrtc::util::CurlHttpClient::create();
    ws_ = nabto::example::RtcWebsocketWrapper::create();
    tf_ = nabto::webrtc::util::StdTimerFactory::create();
    tokGen_ = TestTokenGen::create(accessToken_);
    auto self = shared_from_this();

    conf_ = {deviceId_, productId_, tokGen_, epUrl_, ws_, http_, tf_};

    sig_ = nabto::webrtc::SignalingDeviceFactory::create(conf_);
    return sig_;
  }

  nabto::webrtc::SignalingDevicePtr createConnectedDevice() {
    auto dev = createDevice();
    std::promise<void> connProm;
    dev->addStateChangeListener(
        [&connProm](nabto::webrtc::SignalingDeviceState state) {
          if (state == nabto::webrtc::SignalingDeviceState::CONNECTED) {
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

    res.device->addNewChannelListener(
        [&res, &prom](nabto::webrtc::SignalingChannelPtr conn,
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
    std::vector<std::shared_ptr<org::openapitools::client::model::AnyType> >
        msgs;
    auto msg = std::make_shared<org::openapitools::client::model::AnyType>();

    msg->fromJson(web::json::value::string(message));
    msgs.push_back(msg);

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
    std::vector<std::shared_ptr<org::openapitools::client::model::AnyType> >
        msgsLoc;
    for (auto& m : msgs) {
      auto msg = std::make_shared<org::openapitools::client::model::AnyType>();

      msg->fromJson(web::json::value::string(m));
      msgsLoc.push_back(msg);
    }

    auto req = std::make_shared<
        model::PostTestClientByTestIdWait_for_device_messages_request>();
    req->setTimeout(200);
    req->setMessages(msgsLoc);

    try {
      auto resp = api_->postTestDeviceByTestIdClientsByClientIdWaitForMessages(
                          testId_, cliId, req)
                      .get();
      std::vector<std::string> respMsgs;
      for (auto& m : resp->getMessages()) {
        respMsgs.push_back(m->toJson().as_string());
      }
      return respMsgs;
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

  nabto::webrtc::SignalingDevicePtr getDevice() { return sig_; }

 private:
  std::shared_ptr<api::DefaultApi> api_ = nullptr;
  nabto::webrtc::SignalingHttpClientPtr http_;
  nabto::webrtc::SignalingWebsocketPtr ws_;
  nabto::webrtc::SignalingTimerFactoryPtr tf_;
  nabto::webrtc::SignalingDeviceConfig conf_;
  nabto::webrtc::SignalingDevicePtr sig_;
  nabto::webrtc::SignalingTokenGeneratorPtr tokGen_;
};

}  // namespace test
}  // namespace nabto
