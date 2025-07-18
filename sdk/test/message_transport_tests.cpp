#include <nabto/webrtc/util/message_transport.hpp>

#include <gtest/gtest.h>

namespace nabto {
namespace test {

class MockSignaling : public nabto::webrtc::SignalingChannel,
                      public nabto::webrtc::SignalingDevice {
 public:
  MockSignaling() {}

  uint32_t addMessageListener(
      nabto::webrtc::SignalingMessageHandler handler) override {
    msgHandler_ = handler;
    return 0;
  }

  uint32_t addStateChangeListener(
      nabto::webrtc::SignalingChannelStateHandler handler) override {
    return 0;
  }
  uint32_t addErrorListener(
      nabto::webrtc::SignalingErrorHandler handler) override {
    return 0;
  }

  void removeMessageListener(nabto::webrtc::MessageListenerId id) override {}

  // void removeStateChangeListener(
  //     nabto::webrtc::ChannelStateListenerId id) override {}

  void removeErrorListener(nabto::webrtc::ChannelErrorListenerId id) override {}

  void sendMessage(const nlohmann::json& message) override {
    messages_.push_back(message);
  }

  void sendError(const nabto::webrtc::SignalingError& error) override {
    errors_.push_back(error);
  }

  void close() override {}
  std::string getChannelId() override { return ""; }

  void start() override {}
  void checkAlive() override {}
  void requestIceServers(nabto::webrtc::IceServersResponse callback) override {
    iceCb_ = callback;
  }
  uint32_t addNewChannelListener(
      nabto::webrtc::NewSignalingChannelHandler handler) override {
    chanHandler_ = handler;
    return 0;
  }

  uint32_t addStateChangeListener(
      nabto::webrtc::SignalingDeviceStateHandler handler) override {
    return 0;
  }
  uint32_t addReconnectListener(
      nabto::webrtc::SignalingReconnectHandler handler) override {
    return 0;
  }

  void removeNewChannelListener(
      nabto::webrtc::NewChannelListenerId id) override {}

  void removeStateChangeListener(
      nabto::webrtc::ConnectionStateListenerId id) override {}

  void removeReconnectListener(nabto::webrtc::ReconnectListenerId id) override {
  }

  nabto::webrtc::SignalingMessageHandler msgHandler_ = nullptr;
  nabto::webrtc::NewSignalingChannelHandler chanHandler_ = nullptr;
  std::vector<nlohmann::json> messages_;
  std::vector<nabto::webrtc::SignalingError> errors_;
  nabto::webrtc::IceServersResponse iceCb_ = nullptr;
};

}  // namespace test
}  // namespace nabto

TEST(message_transport, handle_setup_req) {
  auto mock = std::make_shared<nabto::test::MockSignaling>();
  nabto::webrtc::util::MessageTransportPtr mt =
      nabto::webrtc::util::MessageTransportFactory::createNoneTransport(mock,
                                                                        mock);

  ASSERT_TRUE(mock->msgHandler_ != nullptr);

  bool setupDone = false;
  mt->addSetupDoneListener(
      [&setupDone](
          const std::vector<struct nabto::webrtc::IceServer>& servers) {
        ASSERT_EQ(servers.size(), 1);
        auto ice = servers[0];
        ASSERT_EQ(ice.username, "foo");
        ASSERT_EQ(ice.credential, "bar");
        ASSERT_EQ(ice.urls.size(), 1);
        ASSERT_EQ(ice.urls[0], "foobar");
        setupDone = true;
      });

  nlohmann::json setupReq = {{"type", "SETUP_REQUEST"}};
  nlohmann::json signedReq = {{"type", "NONE"}, {"message", setupReq}};
  mock->msgHandler_(signedReq);
  ASSERT_TRUE(mock->errors_.size() == 0);
  ASSERT_TRUE(mock->iceCb_ != nullptr);

  struct nabto::webrtc::IceServer ice = {"foo", "bar", {"foobar"}};
  std::vector<struct nabto::webrtc::IceServer> servs;
  servs.push_back(ice);
  mock->iceCb_(servs);
  ASSERT_TRUE(setupDone);
}

TEST(message_transport, setup_done_with_stun) {
  auto mock = std::make_shared<nabto::test::MockSignaling>();
  nabto::webrtc::util::MessageTransportPtr mt =
      nabto::webrtc::util::MessageTransportFactory::createNoneTransport(mock,
                                                                        mock);

  ASSERT_TRUE(mock->msgHandler_ != nullptr);

  bool setupDone = false;
  mt->addSetupDoneListener(
      [&setupDone](
          const std::vector<struct nabto::webrtc::IceServer>& servers) {
        ASSERT_EQ(servers.size(), 1);
        auto ice = servers[0];
        ASSERT_TRUE(ice.username.empty());
        ASSERT_TRUE(ice.credential.empty());
        ASSERT_EQ(ice.urls.size(), 1);
        ASSERT_EQ(ice.urls[0], "foobar");
        setupDone = true;
      });

  nlohmann::json setupReq = {{"type", "SETUP_REQUEST"}};
  nlohmann::json signedReq = {{"type", "NONE"}, {"message", setupReq}};
  mock->msgHandler_(signedReq);
  ASSERT_TRUE(mock->errors_.size() == 0);
  ASSERT_TRUE(mock->iceCb_ != nullptr);

  struct nabto::webrtc::IceServer ice = {"", "", {"foobar"}};
  std::vector<struct nabto::webrtc::IceServer> servs;
  servs.push_back(ice);
  mock->iceCb_(servs);
  ASSERT_TRUE(setupDone);
}

TEST(message_transport, invalid_setup_req) {
  auto mock = std::make_shared<nabto::test::MockSignaling>();
  nabto::webrtc::util::MessageTransportPtr mt =
      nabto::webrtc::util::MessageTransportFactory::createNoneTransport(mock,
                                                                        mock);

  bool errorHandled = false;
  mt->addErrorListener(
      [&errorHandled](const nabto::webrtc::SignalingError& err) {
        errorHandled = true;
        ASSERT_EQ(err.errorCode(), "DECODE_ERROR");
        ASSERT_EQ(err.errorMessage(),
                  "Could not decode the incoming signaling message");
      });

  ASSERT_TRUE(mock->msgHandler_ != nullptr);

  nlohmann::json setupReq = {{"foo", "bar"}};
  nlohmann::json signedReq = {{"type", "NONE"}, {"message", setupReq}};
  mock->msgHandler_(signedReq);
  ASSERT_EQ(mock->errors_.size(), 1);
  ASSERT_EQ(mock->errors_[0].errorCode(), "DECODE_ERROR");
  ASSERT_EQ(mock->errors_[0].errorMessage(),
            "Could not decode the incoming signaling message");
  ASSERT_TRUE(errorHandled);
}

TEST(message_transport, invalid_signer_type) {
  auto mock = std::make_shared<nabto::test::MockSignaling>();
  nabto::webrtc::util::MessageTransportPtr mt =
      nabto::webrtc::util::MessageTransportFactory::createNoneTransport(mock,
                                                                        mock);

  ASSERT_TRUE(mock->msgHandler_ != nullptr);

  nlohmann::json setupReq = {{"type", "SETUP_REQUEST"}};
  nlohmann::json signedReq = {{"type", "JWT"}, {"message", setupReq}};
  mock->msgHandler_(signedReq);
  ASSERT_EQ(mock->errors_.size(), 1);
  ASSERT_EQ(mock->errors_[0].errorCode(), "VERIFICATION_ERROR");
  ASSERT_EQ(mock->errors_[0].errorMessage(),
            "Could not verify the incoming signaling message");
}

TEST(message_transport, missing_signer_type) {
  auto mock = std::make_shared<nabto::test::MockSignaling>();
  nabto::webrtc::util::MessageTransportPtr mt =
      nabto::webrtc::util::MessageTransportFactory::createNoneTransport(mock,
                                                                        mock);

  ASSERT_TRUE(mock->msgHandler_ != nullptr);

  nlohmann::json setupReq = {{"type", "SETUP_REQUEST"}};
  nlohmann::json signedReq = {{"notType", "NONE"}, {"message", setupReq}};
  mock->msgHandler_(signedReq);
  ASSERT_EQ(mock->errors_.size(), 1);
  ASSERT_EQ(mock->errors_[0].errorCode(), "DECODE_ERROR");
  ASSERT_EQ(mock->errors_[0].errorMessage(),
            "Could not decode the incoming signaling message");
}
