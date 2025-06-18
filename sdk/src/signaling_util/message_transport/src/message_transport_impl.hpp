#include "message_signer.hpp"

#include <nabto/webrtc/util/message_transport.hpp>

#include <map>
#include <mutex>

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
      MessageTransportSharedSecretHandler sharedSecretHandler);

  MessageTransportImpl(signaling::SignalingDevicePtr device,
                       signaling::SignalingChannelPtr channel);
  MessageTransportImpl(signaling::SignalingDevicePtr device,
                       signaling::SignalingChannelPtr channel,
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
      signaling::SignalingMessageHandler handler) override;

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
      signaling::SignalingErrorHandler handler) override;

  void removeErrorListener(TransportErrorListenerId id) override {
    const std::lock_guard<std::mutex> lock(handlerLock_);
    errHandlers_.erase(id);
  }

  void sendMessage(const nlohmann::json& message) override;

 private:
  signaling::SignalingDevicePtr device_;
  signaling::SignalingChannelPtr channel_;
  MessageSignerPtr signer_;

  std::map<TransportMessageListenerId, signaling::SignalingMessageHandler>
      msgHandlers_;
  std::map<TransportErrorListenerId, signaling::SignalingErrorHandler>
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
      const std::vector<struct nabto::signaling::IceServer>& iceServers);
  void handleError(const signaling::SignalingError& err);
};

}  // namespace util
}  // namespace nabto
