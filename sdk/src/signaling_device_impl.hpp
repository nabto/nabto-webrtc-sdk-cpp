#pragma once
#include "signaling_impl.hpp"
#include "websocket_connection.hpp"

#include <nabto/signaling/signaling.hpp>

#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace nabto {
namespace signaling {

class SignalingChannelImpl;
using SignalingChannelImplPtr = std::shared_ptr<SignalingChannelImpl>;

class SignalingDeviceImpl;
using SignalingDeviceImplPtr = std::shared_ptr<SignalingDeviceImpl>;

class SignalingDeviceImpl : public SignalingDevice, public std::enable_shared_from_this<SignalingDeviceImpl> {
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
    void setNewChannelHandler(NewSignalingChannelHandler handler) override {
        channelHandler_ = handler;
    }
    void setStateChangeHandler(SignalingDeviceStateHandler handler) override {
        eventHandler_ = handler;
    };
    void setReconnectHandler(SignalingReconnectHandler handler) override {
        reconnectHandler_ = handler;
    };

    // #### END OF SDK FUNCTIONS ####

    // #### INTERNAL FUNCTIONS ####
    void websocketSendMessage(const std::string& channelId, const nlohmann::json& message);
    void websocketSendError(const std::string& channelId, const SignalingError& error);

    void channelClosed(const std::string& channelId);

   private:
    SignalingWebsocketPtr wsImpl_;
    SignalingHttpClientPtr httpCli_;
    std::map<std::string, SignalingChannelImplPtr> channels_;  // channel ID to channel impl
    NewSignalingChannelHandler channelHandler_;
    SignalingDeviceStateHandler eventHandler_;
    SignalingReconnectHandler reconnectHandler_;
    std::string deviceId_;
    std::string productId_;
    SignalingTokenGeneratorPtr tokenProvider_;
    std::string httpHost_;
    bool closed_ = false;
    bool firstConnect_ = true;
    SignalingDeviceState state_ = SignalingDeviceState::NEW;
    std::mutex mutex_;

    // WS STUFF
    WebsocketConnectionPtr ws_;
    size_t reconnectCounter_ = 0;
    SignalingTimerFactoryPtr timerFactory_;
    SignalingTimerPtr timer_;

    std::string wsUrl_;

    void doConnect();
    void connectWs();
    void handleWsMessage(SignalingMessageType type, const nlohmann::json& message);

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
