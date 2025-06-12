#include "message_signer.hpp"

#include <nabto/webrtc/util/message_transport.hpp>

namespace nabto {
namespace util {

class MessageTransportImpl
    : public MessageTransport,
      public std::enable_shared_from_this<MessageTransportImpl> {
 public:
  enum class SigningMode : std::uint8_t { SHARED_SECRET, NONE };

  static MessageTransportPtr createNone(signaling::SignalingDevicePtr device,
                                        signaling::SignalingChannelPtr channel);
  static MessageTransportPtr createSharedSecret(
      signaling::SignalingDevicePtr device,
      signaling::SignalingChannelPtr channel,
      std::function<std::string(const std::string keyId)> sharedSecretHandler);

  MessageTransportImpl(signaling::SignalingDevicePtr device,
                       signaling::SignalingChannelPtr channel);
  MessageTransportImpl(
      signaling::SignalingDevicePtr device,
      signaling::SignalingChannelPtr channel,
      std::function<std::string(const std::string keyId)> sharedSecretHandler);

  void setSetupDoneHandler(
      std::function<void(const std::vector<signaling::IceServer>& iceServers)>
          handler) override;

  /**
   * Set a handler to be invoked whenever a message is available on the
   * connection
   *
   * @param handler the handler to set
   */
  void setMessageHandler(signaling::SignalingMessageHandler handler) override;

  /**
   * Set a handler to be invoked if an error occurs on the connection
   *
   * @param handler the handler to set
   */
  void setErrorHandler(signaling::SignalingErrorHandler handler) override;

  void sendMessage(const nlohmann::json& message) override;

 private:
  signaling::SignalingDevicePtr device_;
  signaling::SignalingChannelPtr channel_;
  MessageSignerPtr signer_;

  signaling::SignalingMessageHandler msgHandler_ = nullptr;
  signaling::SignalingErrorHandler errHandler_ = nullptr;
  std::function<std::string(const std::string keyId)> secretHandler_ = nullptr;
  std::function<void(const std::vector<signaling::IceServer>& iceServers)>
      setupHandler_ = nullptr;
  std::function<std::string(const std::string keyId)> sharedSecretHandler_ =
      nullptr;

  enum SigningMode mode_;

  void init();
  void setupSigner(const nlohmann::json& msg);
  void handleMessage(const nlohmann::json& msg);
  void requestIceServers();
  void sendSetupResponse(
      const std::vector<struct nabto::signaling::IceServer>& iceServers);
  void handleError(const signaling::SignalingError& err);
};

}  // namespace util
}  // namespace nabto
