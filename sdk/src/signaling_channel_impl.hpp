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
    static SignalingChannelImplPtr create(SignalingDeviceImplPtr signaler, const std::string& channelId);
    SignalingChannelImpl(SignalingDeviceImplPtr signaler, std::string channelId);

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
    void setStateChangeHandler(SignalingChannelStateHandler handler) override { signalingEventHandler_ = handler; };

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
    void sendMessage(const nlohmann::json& message) override;

    /**
     * Send a signaling error to the client
     *
     * @param errorCode The error to send
     */
    void sendError(const SignalingError& error) override;

    /**
     * Close the Signaling channel
     */
    void close() override;

    std::string getChannelId() override;

    // #### END OF SDK FUNCTIONS ####

    // #### INTERNAL FUNCTIONS ####
    void handleMessage(const nlohmann::json& msg);
    void peerConnected();
    void peerOffline();
    void handleError(const SignalingError& error);
    void wsClosed();

    static bool isInitialMessage(const nlohmann::json& msg);

   private:
    SignalingDeviceImplPtr signaler_;
    std::string channelId_;
    bool isDevice_;
    SignalingMessageHandler signalingMessageHandler_;
    SignalingChannelStateHandler signalingEventHandler_;
    SignalingErrorHandler signalingErrorHandler_;
    uint32_t recvSeq_ = 0;
    uint32_t sendSeq_ = 0;
    std::vector<nlohmann::json> unackedMessages_;

    void sendAck(const nlohmann::json& msg);
    void handleAck(const nlohmann::json& msg);
};

}  // namespace signaling
}  // namespace nabto
