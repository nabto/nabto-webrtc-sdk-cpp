#include "message_signer.hpp"

#include <nabto/webrtc/util/message_transport.hpp>

namespace nabto {
namespace util {

class MessageTransportImpl
    : public MessageTransport,
      public std::enable_shared_from_this<MessageTransportImpl> {
 public:
  static MessageTransportPtr create(signaling::SignalingDevicePtr device,
                                    signaling::SignalingChannelPtr sig,
                                    SecurityMode mode);

  MessageTransportImpl(signaling::SignalingDevicePtr device,
                       signaling::SignalingChannelPtr sig, SecurityMode mode);

  void setSharedSecretHandler(
      std::function<std::string(const std::string keyId)> handler) override;

  void setSetupDoneHandler(
      std::function<void(const std::vector<rtc::IceServer>& iceServers)>
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
  signaling::SignalingChannelPtr sig_;
  MessageSignerPtr signer_;

  signaling::SignalingMessageHandler msgHandler_ = nullptr;
  signaling::SignalingErrorHandler errHandler_ = nullptr;
  std::function<std::string(const std::string keyId)> secretHandler_ = nullptr;
  std::function<void(const std::vector<rtc::IceServer>& iceServers)>
      setupHandler_ = nullptr;
};

}  // namespace util
}  // namespace nabto
