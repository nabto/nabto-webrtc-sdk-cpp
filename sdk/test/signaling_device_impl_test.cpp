#include "../src/signaling_device/src/signaling_device_impl.hpp"

#include <nabto/webrtc/device.hpp>

#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

// The fakes never invoke a callback from inside the call that registers or
// triggers it: The device holds its mutex across open() and sendRequest().

class FakeWebsocket : public nabto::webrtc::SignalingWebsocket {
 public:
  bool send(const std::string& data) override {
    sent.push_back(nlohmann::json::parse(data));
    return true;
  }
  void close() override {}
  void onOpen(std::function<void()> callback) override {
    openCb = std::move(callback);
  }
  void onMessage(
      std::function<void(const std::string& message)> callback) override {
    messageCb = std::move(callback);
  }
  void onClosed(std::function<void()> callback) override {
    closedCb = std::move(callback);
  }
  void onError(
      std::function<void(const std::string& error)> callback) override {
    errorCb = std::move(callback);
  }
  void open(const std::string& /*url*/) override {}

  // The callbacks hold the device, which holds this websocket.
  void dropCallbacks() {
    openCb = nullptr;
    messageCb = nullptr;
    closedCb = nullptr;
    errorCb = nullptr;
  }

  std::vector<nlohmann::json> sent;
  std::function<void()> openCb;
  std::function<void(const std::string&)> messageCb;
  std::function<void()> closedCb;
  std::function<void(const std::string&)> errorCb;
};

class FakeHttpClient : public nabto::webrtc::SignalingHttpClient {
 public:
  bool sendRequest(const nabto::webrtc::SignalingHttpRequest& /*request*/,
                   nabto::webrtc::HttpResponseCallback callback) override {
    pending = std::move(callback);
    return true;
  }
  nabto::webrtc::HttpResponseCallback pending;
};

class FakeTokenGenerator : public nabto::webrtc::SignalingTokenGenerator {
 public:
  bool generateToken(std::string& token) override {
    token = "token";
    return true;
  }
};

// A signaling device attached to a fake websocket, with one listener that
// records every message delivered on any channel.
class ConnectedDevice {
 public:
  ConnectedDevice() {
    nabto::webrtc::SignalingDeviceConfig conf;
    conf.deviceId = "wd-test";
    conf.productId = "wp-test";
    conf.tokenProvider = std::make_shared<FakeTokenGenerator>();
    conf.wsImpl = ws_;
    conf.httpCli = http_;
    device_ = nabto::webrtc::SignalingDeviceImpl::create(conf);
    device_->addNewChannelListener(
        [this](const nabto::webrtc::SignalingChannelPtr& channel,
               bool /*authorized*/) {
          channel->addMessageListener([this](const nlohmann::json& msg) {
            delivered.push_back(msg);
          });
        });
    device_->start();
    auto response = std::make_unique<nabto::webrtc::SignalingHttpResponse>();
    response->statusCode = 200;
    response->body = R"({"signalingUrl":"wss://example.invalid"})";
    http_->pending(std::move(response));
    ws_->openCb();
  }

  ~ConnectedDevice() {
    device_->close();
    ws_->dropCallbacks();
  }

  ConnectedDevice(const ConnectedDevice&) = delete;
  ConnectedDevice& operator=(const ConnectedDevice&) = delete;
  ConnectedDevice(ConnectedDevice&&) = delete;
  ConnectedDevice& operator=(ConnectedDevice&&) = delete;

  // Receive a DATA message from the client as the backend forwards it.
  void receiveData(uint32_t seq, const nlohmann::json& data) {
    const nlohmann::json frame = {
        {"type", "MESSAGE"},
        {"channelId", "chan"},
        {"authorized", true},
        {"message", {{"type", "DATA"}, {"seq", seq}, {"data", data}}}};
    ws_->messageCb(frame.dump());
  }

  // Sequence numbers of the ACKs the device has sent, in order.
  std::vector<uint32_t> ackedSeqs() const {
    std::vector<uint32_t> seqs;
    for (const auto& frame : ws_->sent) {
      if (frame.at("type") == "MESSAGE" &&
          frame.at("message").at("type") == "ACK") {
        seqs.push_back(frame.at("message").at("seq").get<uint32_t>());
      }
    }
    return seqs;
  }

  std::vector<nlohmann::json> delivered;

 private:
  std::shared_ptr<FakeWebsocket> ws_ = std::make_shared<FakeWebsocket>();
  std::shared_ptr<FakeHttpClient> http_ = std::make_shared<FakeHttpClient>();
  nabto::webrtc::SignalingDeviceImplPtr device_;
};

}  // namespace

// A client resends its unacknowledged messages when it sees PEER_CONNECTED,
// so the first message can arrive twice. The duplicate must be acknowledged
// and dropped: Delivering it again makes the message signer reject it and
// fails the channel.
TEST(signaling_device_impl, duplicateInitialMessageIsDeliveredOnce) {
  ConnectedDevice device;
  const nlohmann::json setupRequest = {{"type", "SETUP_REQUEST"}};

  device.receiveData(0, setupRequest);
  device.receiveData(0, setupRequest);

  ASSERT_EQ(device.delivered.size(), 1);
  ASSERT_EQ(device.delivered[0], setupRequest);
  const std::vector<uint32_t> expectedAcks = {0, 0};
  ASSERT_EQ(device.ackedSeqs(), expectedAcks);
}

TEST(signaling_device_impl, retransmittedMessageIsAckedButNotRedelivered) {
  ConnectedDevice device;

  device.receiveData(0, "first");
  device.receiveData(1, "second");
  device.receiveData(0, "first");
  device.receiveData(1, "second");
  device.receiveData(2, "third");

  const std::vector<nlohmann::json> expected = {"first", "second", "third"};
  ASSERT_EQ(device.delivered, expected);
  const std::vector<uint32_t> expectedAcks = {0, 1, 0, 1, 2};
  ASSERT_EQ(device.ackedSeqs(), expectedAcks);
}

// A message ahead of the expected sequence number is neither delivered nor
// acknowledged, so the client retransmits it in order.
TEST(signaling_device_impl, messageFromTheFutureIsIgnoredUntilInOrder) {
  ConnectedDevice device;

  device.receiveData(0, "first");
  device.receiveData(2, "third");
  ASSERT_EQ(device.delivered.size(), 1);
  const std::vector<uint32_t> acksBefore = {0};
  ASSERT_EQ(device.ackedSeqs(), acksBefore);

  device.receiveData(1, "second");
  device.receiveData(2, "third");

  const std::vector<nlohmann::json> expected = {"first", "second", "third"};
  ASSERT_EQ(device.delivered, expected);
  const std::vector<uint32_t> expectedAcks = {0, 1, 2};
  ASSERT_EQ(device.ackedSeqs(), expectedAcks);
}

TEST(signaling_device_impl, parseIceServersTest) {
  std::string iceServers = R"(
    {
        "iceServers": [{
            "urls": ["a"],
            "username": "b",
            "credential": "c"
        }]
    }
    )";
  std::vector<nabto::webrtc::IceServer> result =
      nabto::webrtc::SignalingDeviceImpl::parseIceServers(iceServers);
  ASSERT_EQ(result.size(), 1);
  auto s = result[0];
  ASSERT_EQ(s.credential, "c");
  ASSERT_EQ(s.username, "b");
  {
    std::vector<std::string> expected = {"a"};
    ASSERT_EQ(s.urls, expected);
  }
}

TEST(signaling_device_impl, parseIceServersExtraData) {
  std::string iceServers = R"(
    {
    "iceServers": [
      {
        "urls": ["stun:stun.nabto.net"]
      },
      {
        "urls": [],
        "bar": "baz"
      }
    ]
    }
    )";
  std::vector<nabto::webrtc::IceServer> result =
      nabto::webrtc::SignalingDeviceImpl::parseIceServers(iceServers);
  ASSERT_EQ(result.size(), 2);
  auto s1 = result[0];
  ASSERT_EQ(s1.urls.size(), 1);
  auto u1 = s1.urls[0];
  ASSERT_EQ(u1, "stun:stun.nabto.net");
  ASSERT_TRUE(s1.username.empty());
  ASSERT_TRUE(s1.credential.empty());
  auto s2 = result[1];
  ASSERT_EQ(s2.urls.size(), 0);
  ASSERT_TRUE(s2.username.empty());
  ASSERT_TRUE(s2.credential.empty());
}
