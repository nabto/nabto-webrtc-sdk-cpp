#pragma once
#include "signaling_impl.hpp"

#include <nabto/webrtc/device.hpp>

#include <nlohmann/json.hpp>

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>

namespace nabto {
namespace webrtc {

class SignalingChannelImpl;
using SignalingChannelImplPtr = std::shared_ptr<SignalingChannelImpl>;

class SignalingChannelImpl
    : public SignalingChannel,
      public std::enable_shared_from_this<SignalingChannel> {
 public:
  /*
   * SignalingChannels are created by the Signaler and received by its channel
   * handler
   */
  static SignalingChannelImplPtr create(SignalingDeviceImplPtr signaler,
                                        const std::string& channelId);
  SignalingChannelImpl(SignalingDeviceImplPtr signaler, std::string channelId);

  // #### SDK FUNCTIONS ####
  /**
   * Set a handler to be invoked whenever a message is available on the channel
   *
   * @param handler the handler to set
   */
  MessageListenerId addMessageListener(
      SignalingMessageHandler handler) override {
    const std::lock_guard<std::mutex> lock(mutex_);
    const MessageListenerId id = currMsgListId_;
    currMsgListId_++;
    messageHandlers_.insert({id, handler});
    return id;
  }

  void removeMessageListener(MessageListenerId id) override {
    const std::lock_guard<std::mutex> lock(mutex_);
    messageHandlers_.erase(id);
  }

  /**
   * Set a handler to be invoked when an event occurs on the channel.
   *
   * @param handler the handler to set
   */
  ChannelStateListenerId addStateChangeListener(
      SignalingChannelStateHandler handler) override {
    const std::lock_guard<std::mutex> lock(mutex_);
    const ChannelStateListenerId id = currStateListId_;
    currStateListId_++;
    stateHandlers_.insert({id, handler});
    return id;
  };

  void removeStateChangeListener(ChannelStateListenerId id) override {
    const std::lock_guard<std::mutex> lock(mutex_);
    stateHandlers_.erase(id);
  }

  /**
   * Set a handler to be invoked if an error occurs on the channel
   *
   * @param handler the handler to set
   */
  ChannelErrorListenerId addErrorListener(
      SignalingErrorHandler handler) override {
    const std::lock_guard<std::mutex> lock(mutex_);
    const ChannelErrorListenerId id = currErrListId_;
    currErrListId_++;
    errorHandlers_.insert({id, handler});
    return id;
  };

  void removeErrorListener(ChannelErrorListenerId id) override {
    const std::lock_guard<std::mutex> lock(mutex_);
    errorHandlers_.erase(id);
  }

  /**
   * Send a signaling message to the client
   *
   * @param message The message to send
   */
  void sendMessage(const nlohmann::json& message) override;

  /**
   * Send a signaling error to the client
   *
   * @param errorCode The error to send
   */
  void sendError(const SignalingError& error) override;

  /**
   * Close the Signaling channel
   */
  void close() override;

  std::string getChannelId() override;

  // #### END OF SDK FUNCTIONS ####

  // #### INTERNAL FUNCTIONS ####
  void handleMessage(const nlohmann::json& msg);
  void peerConnected();
  void peerOffline();
  void handleError(const SignalingError& error);
  void wsClosed();

  static bool isInitialMessage(const nlohmann::json& msg);

 private:
  SignalingDeviceImplPtr signaler_;
  std::string channelId_;

  std::map<MessageListenerId, SignalingMessageHandler> messageHandlers_;
  std::map<ChannelStateListenerId, SignalingChannelStateHandler> stateHandlers_;
  std::map<ChannelErrorListenerId, SignalingErrorHandler> errorHandlers_;

  MessageListenerId currMsgListId_ = 0;
  ChannelStateListenerId currStateListId_ = 0;
  ChannelErrorListenerId currErrListId_ = 0;

  uint32_t recvSeq_ = 0;
  uint32_t sendSeq_ = 0;
  std::vector<nlohmann::json> unackedMessages_;
  std::mutex mutex_;
  SignalingChannelState state_ = SignalingChannelState::NEW;

  void sendAck(const nlohmann::json& msg);
  void handleAck(const nlohmann::json& msg);
  void changeState(SignalingChannelState state);

  bool stateIsEnded() {
    return (state_ == SignalingChannelState::CLOSED ||
            state_ == SignalingChannelState::FAILED);
  }
};

}  // namespace webrtc
}  // namespace nabto
