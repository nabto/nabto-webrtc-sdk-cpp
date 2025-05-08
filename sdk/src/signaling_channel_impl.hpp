#pragma once
#include "signaling_impl.hpp"

#include <nabto/signaling/signaling.hpp>

#include <nlohmann/json.hpp>

#include <memory>

namespace nabto {
namespace signaling {

class SignalingChannelImpl;
using SignalingChannelImplPtr = std::shared_ptr<SignalingChannelImpl>;

class SignalingChannelImpl : public SignalingChannel, public std::enable_shared_from_this<SignalingChannel> {
   public:
    /*
     * SignalingChannels are created by the Signaler and received by its channel handler
     */
    static SignalingChannelImplPtr create(SignalingDeviceImplPtr signaler, const std::string& channelId, bool isDevice, bool isAuthorized);
    SignalingChannelImpl(SignalingDeviceImplPtr signaler, std::string channelId, bool isDevice, bool isAuthorized);

    // #### SDK FUNCTIONS ####
    /**
     * Set a handler to be invoked whenever a message is available on the channel
     *
     * @param handler the handler to set
     */
    void setMessageHandler(SignalingMessageHandler handler) override { signalingMessageHandler_ = handler; }

    /**
     * Set a handler to be invoked when an event occurs on the channel.
     *
     * @param handler the handler to set
     */
    void setEventHandler(SignalingChannelEventHandler handler) override { signalingEventHandler_ = handler; };

    /**
     * Set a handler to be invoked if an error occurs on the channel
     *
     * @param handler the handler to set
     */
    void setErrorHandler(SignalingErrorHandler handler) override { signalingErrorHandler_ = handler; };

    /**
     * Send a signaling message to the client
     *
     * @param message The message to send
     */
    void sendMessage(const std::string& message) override;

    /**
     * Send a signaling error to the client
     *
     * @param errorCode The error to send
     */
    void sendError(const SignalingError& error) override;

    /**
     * Trigger the Signaler to validate that its Websocket connection is alive. If the connection is still alive, nothing happens. Otherwise, the Signaler will reconnect to the backend and trigger a Signaling Event.
     */
    void checkAlive() override;

    /**
     * Request ICE servers from the Nabto Backend
     *
     * @param callback callback to be invoked when the request is resolved
     */
    void requestIceServers(IceServersResponse callback) override;

    /**
     * Close the Signaling channel
     */
    void close() override;

    bool isDevice() override { return isDevice_; }

    bool isAuthorized() override { return isAuthorized_; }

    // #### END OF SDK FUNCTIONS ####

    // #### INTERNAL FUNCTIONS ####
    void handleMessage(const std::string& msg);
    void wsReconnected();
    void peerConnected();
    void peerOffline();
    void handleError(const SignalingError& error);
    void wsClosed();

    static bool isInitialMessage(const std::string& msg);

   private:
    SignalingDeviceImplPtr signaler_;
    std::string channelId_;
    bool isDevice_;
    bool isAuthorized_;
    SignalingMessageHandler signalingMessageHandler_;
    SignalingChannelEventHandler signalingEventHandler_;
    SignalingErrorHandler signalingErrorHandler_;
    uint32_t recvSeq_ = 0;
    uint32_t sendSeq_ = 0;
    std::vector<nlohmann::json> unackedMessages_;

    void sendAck(nlohmann::json& msg);
    void handleAck(nlohmann::json& msg);
};

}  // namespace signaling
}  // namespace nabto
