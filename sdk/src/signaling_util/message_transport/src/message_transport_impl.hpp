#include "message_signer.hpp"

#include <nabto/webrtc/util/message_transport.hpp>

#include <map>
#include <mutex>

namespace nabto {
namespace webrtc {
namespace util {

class MessageTransportImpl
    : public MessageTransport,
      public std::enable_shared_from_this<MessageTransportImpl> {
 public:
  enum class SigningMode : std::uint8_t { SHARED_SECRET, NONE };

  static MessageTransportPtr createNone(
      nabto::webrtc::SignalingDevicePtr device,
      nabto::webrtc::SignalingChannelPtr channel);
  static MessageTransportPtr createSharedSecret(
      nabto::webrtc::SignalingDevicePtr device,
      nabto::webrtc::SignalingChannelPtr channel,
      MessageTransportSharedSecretHandler sharedSecretHandler);

  MessageTransportImpl(nabto::webrtc::SignalingDevicePtr device,
                       nabto::webrtc::SignalingChannelPtr channel);
  MessageTransportImpl(nabto::webrtc::SignalingDevicePtr device,
                       nabto::webrtc::SignalingChannelPtr channel,
                       MessageTransportSharedSecretHandler sharedSecretHandler);

  SetupDoneListenerId addSetupDoneListener(SetupDoneHandler handler) override;

  void removeSetupDoneListener(SetupDoneListenerId id) override {
    const std::lock_guard<std::mutex> lock(handlerLock_);
    setupHandlers_.erase(id);
  }

  /**
   * Set a handler to be invoked whenever a message is available on the
   * connection
   *
   * @param handler the handler to set
   */
  TransportMessageListenerId addMessageListener(
      nabto::webrtc::SignalingMessageHandler handler) override;

  void removeMessageListener(TransportMessageListenerId id) override {
    const std::lock_guard<std::mutex> lock(handlerLock_);
    msgHandlers_.erase(id);
  }
  /**
   * Set a handler to be invoked if an error occurs on the connection
   *
   * @param handler the handler to set
   */
  TransportErrorListenerId addErrorListener(
      nabto::webrtc::SignalingErrorHandler handler) override;

  void removeErrorListener(TransportErrorListenerId id) override {
    const std::lock_guard<std::mutex> lock(handlerLock_);
    errHandlers_.erase(id);
  }

  void sendMessage(const nlohmann::json& message) override;

 private:
  nabto::webrtc::SignalingDevicePtr device_;
  nabto::webrtc::SignalingChannelPtr channel_;
  MessageSignerPtr signer_;

  std::map<TransportMessageListenerId, nabto::webrtc::SignalingMessageHandler>
      msgHandlers_;
  std::map<TransportErrorListenerId, nabto::webrtc::SignalingErrorHandler>
      errHandlers_;
  std::map<SetupDoneListenerId, SetupDoneHandler> setupHandlers_;

  TransportMessageListenerId currMsgListId_ = 0;
  TransportErrorListenerId currErrListId_ = 0;
  SetupDoneListenerId currSetupListId_ = 0;

  MessageTransportSharedSecretHandler sharedSecretHandler_ = nullptr;

  std::mutex handlerLock_;

  enum SigningMode mode_;

  void init();
  void setupSigner(const nlohmann::json& msg);
  void handleMessage(const nlohmann::json& msg);
  void requestIceServers();
  void sendSetupResponse(
      const std::vector<struct nabto::webrtc::IceServer>& iceServers);
  void handleError(const nabto::webrtc::SignalingError& err);
};

}  // namespace util
}  // namespace webrtc
}  // namespace nabto
