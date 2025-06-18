#pragma once

#include <rtc/rtc.hpp>

#include <nabto/webrtc/device.hpp>

#include <nlohmann/json.hpp>

#include <memory>

namespace nabto {
namespace util {

class MessageTransport;
using MessageTransportPtr = std::shared_ptr<MessageTransport>;

class MessageTransportFactory {
 public:
  static MessageTransportPtr createSharedSecretTransport(
      signaling::SignalingDevicePtr device, signaling::SignalingChannelPtr sig,
      std::function<std::string(const std::string keyId)> handler);

  static MessageTransportPtr createNoneTransport(
      signaling::SignalingDevicePtr device, signaling::SignalingChannelPtr sig);
};

using SetupDoneHandler =
    std::function<void(const std::vector<signaling::IceServer>& iceServers)>;

using SetupDoneListenerId = uint32_t;
using TransportMessageListenerId = uint32_t;
using TransportErrorListenerId = uint32_t;

class MessageTransport {
 public:
  virtual ~MessageTransport() = default;
  MessageTransport() = default;
  MessageTransport(const MessageTransport&) = delete;
  MessageTransport& operator=(const MessageTransport&) = delete;
  MessageTransport(MessageTransport&&) = delete;
  MessageTransport& operator=(MessageTransport&&) = delete;

  virtual SetupDoneListenerId addSetupDoneListener(
      SetupDoneHandler handler) = 0;

  virtual void removeSetupDoneListener(SetupDoneListenerId id) = 0;
  /**
   * Set a handler to be invoked whenever a message is available on the
   * connection
   *
   * @param handler the handler to set
   */
  virtual TransportMessageListenerId addMessageListener(
      signaling::SignalingMessageHandler handler) = 0;

  virtual void removeMessageListener(TransportMessageListenerId id) = 0;

  /**
   * Set a handler to be invoked if an error occurs on the connection
   *
   * @param handler the handler to set
   */
  virtual TransportErrorListenerId addErrorListener(
      signaling::SignalingErrorHandler handler) = 0;

  virtual void removeErrorListener(TransportErrorListenerId id) = 0;

  virtual void sendMessage(const nlohmann::json& message) = 0;
};

}  // namespace util
}  // namespace nabto
