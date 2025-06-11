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

class MessageTransport {
 public:
  virtual void setSetupDoneHandler(
      std::function<void(const std::vector<signaling::IceServer>& iceServers)>
          handler) = 0;

  /**
   * Set a handler to be invoked whenever a message is available on the
   * connection
   *
   * @param handler the handler to set
   */
  virtual void setMessageHandler(
      signaling::SignalingMessageHandler handler) = 0;

  /**
   * Set a handler to be invoked if an error occurs on the connection
   *
   * @param handler the handler to set
   */
  virtual void setErrorHandler(signaling::SignalingErrorHandler handler) = 0;

  virtual void sendMessage(const nlohmann::json& message) = 0;
};

}  // namespace util
}  // namespace nabto
