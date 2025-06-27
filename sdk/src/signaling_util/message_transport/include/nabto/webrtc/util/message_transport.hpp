#pragma once

#include <rtc/rtc.hpp>

#include <nabto/webrtc/device.hpp>

#include <nlohmann/json.hpp>

#include <memory>

namespace nabto {
namespace webrtc {
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
 * Class representing a WebRTC Description received or to be sent on the
 * MessageTransport.
 */
class SignalingDescription {
 public:
  /**
   * Construct a SignalingDescription object to send
   *
   * @param descType type of the description, typically "offer" or "answer"
   * @param descSdp SDP representation of the description.
   */
  SignalingDescription(const std::string& descType, const std::string& descSdp);

  /**
   * Convert the description to JSON format following the network protocol.
   *
   * @return The JSON document
   */
  nlohmann::json toJson();

  /**
   * The description type (typically "offer" or "answer")
   */
  std::string type;

  /**
   * SDP of the description.
   */
  std::string sdp;
};

/**
 * Class representing a WebRTC ICE Candidate received or to be sent on the
 * MessageTransport.
 */
class SignalingCandidate {
 public:
  /**
   * Construct a Candidate to be sent by the MessageTransport.
   *
   * @param cand The string representation of the candidate.
   */
  SignalingCandidate(const std::string& cand);

  /**
   * Set the optional SDP MID value of a candidate.
   *
   * @param mid The MID to set.
   */
  void setSdpMid(const std::string& mid);

  /**
   * Set the optional SDP M Line Index of the candidate
   *
   * @param index The index to set.
   */
  void setSdpMLineIndex(int index);

  /**
   * Set the optional username fragment of the candidate
   *
   * @param ufrag The username fragment to set.
   */
  void setUsernameFragment(const std::string& ufrag);

  /**
   * Convert the description to JSON format following the network protocol.
   *
   * @return The JSON document
   */
  nlohmann::json toJson();

  /**
   * The string representation of the candidate
   */
  std::string candidate;

  /**
   * Optional SDP MID of the candidate. If this is empty, it means the value
   * does not exist.
   */
  std::string sdpMid;

  /**
   * Optional SDP M Line Index of the candidate. If this is `<0` it means the
   * value does not exist.
   */
  int sdpMLineIndex;

  /**
   * Optional Username Fragment of the candidate. If this is empty, it means the
   * value does not exist.
   */
  std::string usernameFragment;
};

/**
 * Generalized WebRTC Signaling message to be sent/received by the
 * MessageTransport. This message can contain either a SignalingDescription or a
 * SignalingCandidate.
 */
class WebrtcSignalingMessage {
 public:
  /**
   * Construct a WebRTC Signaling message from the JSON format defined by the
   * protocol.
   *
   * If the JSON is invalid or not following the protocol, this will throw an
   * exception.
   *
   * @param jsonMessage The message to decode.
   * @return The created signaling message.
   */
  static WebrtcSignalingMessage fromJson(nlohmann::json& jsonMessage);

  /**
   * Construct a WebRTC Signaling message from a SignalingDescription.
   *
   * @param description The description to construct with.
   */
  WebrtcSignalingMessage(SignalingDescription description);

  /**
   * Construct a WebRTC Signaling message from a SignalingCandidate.
   *
   * @param candidate The candidate to construct with.
   */
  WebrtcSignalingMessage(SignalingCandidate candidate);

  /**
   * Check if the message is a SignalingDescription.
   *
   * @return True iff the message contains a description.
   */
  bool isDescription() const;

  /**
   * Check if the message is a SignalingCandidate.
   *
   * @return True iff the message contains a candidate.
   */
  bool isCandidate() const;

  /**
   * Get the SignalingDescription contained in this message if isDescription()
   * returns true.
   *
   * @return The contained SignalingDescription object.
   */
  SignalingDescription getDescription() const;

  /**
   * Get the SignalingCandidate contained in this message if isCandidate()
   * returns true.
   *
   * @return The contained SignalingCandidate object.
   */
  SignalingCandidate getCandidate() const;

 private:
  std::unique_ptr<SignalingDescription> description_ = nullptr;
  std::unique_ptr<SignalingCandidate> candidate_ = nullptr;
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

/**
 * Handler invoked when a new message is available from the MessageTransport.
 *
 * @param message The new message.
 */
using MessageTransportMessageHandler =
    std::function<void(WebrtcSignalingMessage& message)>;

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
      MessageTransportMessageHandler handler) = 0;

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
  virtual void sendMessage(const WebrtcSignalingMessage& message) = 0;
};

}  // namespace util
}  // namespace webrtc
}  // namespace nabto
