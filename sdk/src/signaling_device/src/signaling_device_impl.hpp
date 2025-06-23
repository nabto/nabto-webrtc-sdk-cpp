#pragma once
#include "signaling_impl.hpp"
#include "websocket_connection.hpp"

#include <nabto/webrtc/device.hpp>

#include <cstddef>
#include <memory>
#include <mutex>
#include <string>

namespace nabto {
namespace signaling {

class SignalingChannelImpl;
using SignalingChannelImplPtr = std::shared_ptr<SignalingChannelImpl>;

class SignalingDeviceImpl;
using SignalingDeviceImplPtr = std::shared_ptr<SignalingDeviceImpl>;

class SignalingDeviceImpl
    : public SignalingDevice,
      public std::enable_shared_from_this<SignalingDeviceImpl> {
 public:
  // #### SDK FUNCTIONS ####
  /**
   * Create a Signaler
   */
  static SignalingDeviceImplPtr create(const SignalingDeviceConfig& conf);
  explicit SignalingDeviceImpl(const SignalingDeviceConfig& conf);

  void start() override;
  void close() override;
  void checkAlive() override;
  void requestIceServers(IceServersResponse callback) override;

  NewChannelListenerId addNewChannelListener(
      NewSignalingChannelHandler handler) override {
    const std::lock_guard<std::mutex> lock(mutex_);
    const NewChannelListenerId id = currChanListId_;
    currChanListId_++;
    chanHandlers_.insert({id, handler});
    return id;
  }

  void removeNewChannelListener(NewChannelListenerId id) override {
    const std::lock_guard<std::mutex> lock(mutex_);
    chanHandlers_.erase(id);
  }

  ConnectionStateListenerId addStateChangeListener(
      SignalingDeviceStateHandler handler) override {
    const std::lock_guard<std::mutex> lock(mutex_);
    const ChannelStateListenerId id = currStateListId_;
    currStateListId_++;
    stateHandlers_.insert({id, handler});
    return id;
  };

  void removeStateChangeListener(ConnectionStateListenerId id) override {
    const std::lock_guard<std::mutex> lock(mutex_);
    stateHandlers_.erase(id);
  }

  ReconnectListenerId addReconnectListener(
      SignalingReconnectHandler handler) override {
    const std::lock_guard<std::mutex> lock(mutex_);
    const ReconnectListenerId id = currReconnListId_;
    currReconnListId_++;
    reconnHandlers_.insert({id, handler});
    return id;
  };

  void removeReconnectListener(ReconnectListenerId id) override {
    const std::lock_guard<std::mutex> lock(mutex_);
    reconnHandlers_.erase(id);
  }

  // #### END OF SDK FUNCTIONS ####

  // #### INTERNAL FUNCTIONS ####
  void websocketSendMessage(const std::string& channelId,
                            const nlohmann::json& message);
  void websocketSendError(const std::string& channelId,
                          const SignalingError& error);

  void channelClosed(const std::string& channelId);

 private:
  SignalingWebsocketPtr wsImpl_;
  SignalingHttpClientPtr httpCli_;
  std::map<std::string, SignalingChannelImplPtr>
      channels_;  // channel ID to channel impl

  std::map<NewChannelListenerId, NewSignalingChannelHandler> chanHandlers_;
  std::map<ConnectionStateListenerId, SignalingDeviceStateHandler>
      stateHandlers_;
  std::map<ReconnectListenerId, SignalingReconnectHandler> reconnHandlers_;

  NewChannelListenerId currChanListId_ = 0;
  ConnectionStateListenerId currStateListId_ = 0;
  ReconnectListenerId currReconnListId_ = 0;

  std::string deviceId_;
  std::string productId_;
  SignalingTokenGeneratorPtr tokenProvider_;
  std::string httpHost_;
  bool closed_ = false;
  bool firstConnect_ = true;
  SignalingDeviceState state_ = SignalingDeviceState::NEW;
  std::mutex mutex_;

  void deinit();

  // WS STUFF
  WebsocketConnectionPtr ws_;
  size_t reconnectCounter_ = 0;
  SignalingTimerFactoryPtr timerFactory_;
  SignalingTimerPtr timer_;

  std::string wsUrl_;

  void doConnect();
  void connectWs();
  void handleWsMessage(SignalingMessageType type,
                       const nlohmann::json& message);

  void sendPong();
  void waitReconnect();
  void changeState(SignalingDeviceState state);

  // HTTP STUFF

  void parseAttachResponse(const std::string& response);
  std::string DEFAULT_SIGNALING_DOMAIN = ".webrtc.nabto.net";

 public:
  static std::vector<struct IceServer> parseIceServers(const std::string& data);
};

}  // namespace signaling
}  // namespace nabto
