#pragma once

#include <rtc/rtc.hpp>

#include <nabto/webrtc/device.hpp>

#include <nlohmann/json.hpp>

#include <memory>

namespace nabto {
namespace util {

class MessageTransport;
using MessageTransportPtr = std::shared_ptr<MessageTransport>;

/**
 * Handler function invoked when a new Shared Secret Transport has received its
 * first message. The message transport will read the Key ID from the JWT header
 * and pass it to this handler. If the JWT does not contain the kid header, the
 * keyId string will be empty. The handler must return a shared secret string
 * matching the key ID to use for the shared secret transport.
 *
 * @param keyId The value of the JWT kid header. Empty if the header does not
 * exist
 * @return The shared secret corresponding to the key ID
 */
using MessageTransportSharedSecretHandler =
    std::function<std::string(const std::string keyId)>;

/**
 * Factory class for constructing MessageTransports for either Shared Secret
 * Transport or None Transport.
 */
class MessageTransportFactory {
 public:
  /**
   * Create a MessageTransport object for Shared Secret transport.
   *
   * @param device The SignalingDevice context.
   * @param channel The SignalingChannel context.
   * @param handler Handler function to retrieve the Shared Secret to use.
   */
  static MessageTransportPtr createSharedSecretTransport(
      nabto::webrtc::SignalingDevicePtr device,
      nabto::webrtc::SignalingChannelPtr channel,
      MessageTransportSharedSecretHandler handler);

  /**
   * Create a MessageTransport object for None transport.
   *
   * @param device The SignalingDevice context.
   * @param channel The SignalingChannel context.
   */
  static MessageTransportPtr createNoneTransport(
      nabto::webrtc::SignalingDevicePtr device,
      nabto::webrtc::SignalingChannelPtr channel);
};

/**
 * Handler invoked when the MessageTransport has completed the setup phase of a
 * channel.
 *
 * @param iceServers The list of ICE server configurations returned by the Nabto
 * Backend.
 */
using SetupDoneHandler = std::function<void(
    const std::vector<nabto::webrtc::IceServer>& iceServers)>;

using SetupDoneListenerId = uint32_t;
using TransportMessageListenerId = uint32_t;
using TransportErrorListenerId = uint32_t;

/**
 * Class used to handle signing, verification, encoding, and decoding of
 * messages sent and received on a SignalingChannel.
 */
class MessageTransport {
 public:
  virtual ~MessageTransport() = default;
  MessageTransport() = default;
  MessageTransport(const MessageTransport&) = delete;
  MessageTransport& operator=(const MessageTransport&) = delete;
  MessageTransport(MessageTransport&&) = delete;
  MessageTransport& operator=(MessageTransport&&) = delete;

  /**
   * Add listener for when the setup phase is done and messages can be sent.
   *
   * @param handler Handler to add.
   * @return ID of the added handler to be used when removing it.
   */
  virtual SetupDoneListenerId addSetupDoneListener(
      SetupDoneHandler handler) = 0;

  /**
   * Remove listener for new Signaling Channels.
   *
   * @param id The ID returned when adding the listener.
   */
  virtual void removeSetupDoneListener(SetupDoneListenerId id) = 0;

  /**
   * Add listener for when a new message is received.
   *
   * @param handler Handler to add.
   * @return ID of the added handler to be used when removing it.
   */
  virtual TransportMessageListenerId addMessageListener(
      nabto::webrtc::SignalingMessageHandler handler) = 0;

  /**
   * Remove message listener.
   *
   * @param id The ID returned when adding the listener.
   */
  virtual void removeMessageListener(TransportMessageListenerId id) = 0;

  /**
   * Add listener for error events occurring in the MessageTransport module.
   *
   * This only reports errors in the MessageTransport. To get errors from the
   * SignalingChannel a listener must be added there.
   *
   * @param handler Handler to add.
   * @return ID of the added handler to be used when removing it.
   */
  virtual TransportErrorListenerId addErrorListener(
      nabto::webrtc::SignalingErrorHandler handler) = 0;

  /**
   * Remove an error listener.
   *
   * @param id The ID returned when adding the listener.
   */
  virtual void removeErrorListener(TransportErrorListenerId id) = 0;

  /**
   * Send a message through the Message Transport.
   *
   * This will cause the Message Transport to encode and sign the message before
   * sending it on the signaling channel.
   *
   * @param message The message to send.
   */
  virtual void sendMessage(const nlohmann::json& message) = 0;
};

}  // namespace util
}  // namespace nabto
