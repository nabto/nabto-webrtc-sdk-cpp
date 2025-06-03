#include "websocket_connection.hpp"

#include "logging.hpp"
#include "signaling_impl.hpp"

#include <nlohmann/json_fwd.hpp>

#include <cstddef>
#include <exception>
#include <functional>
#include <stdexcept>
#include <string>
#include <utility>

namespace nabto {
namespace signaling {

bool WebsocketConnection::send(const std::string& data) {
    return ws_->send(data);
}

void WebsocketConnection::close() {
    ws_->close();
}

void WebsocketConnection::onOpen(std::function<void()> callback) {
    ws_->onOpen(std::move(callback));
}

void WebsocketConnection::onMessage(const std::function<void(SignalingMessageType type, nlohmann::json& message)>& callback) {
    auto self = shared_from_this();
    ws_->onMessage([self, callback](const std::string& msg) {
        try {
            auto root = nlohmann::json::parse(msg);
            auto type = parseWsMsgType(root["type"].get<std::string>());
            if (type == SignalingMessageType::PONG) {
                self->handlePong();
            } else {
                callback(type, root);
            }
        } catch (std::exception& ex) {
            NABTO_SIGNALING_LOGE << "Failed parse websocket message: " << msg << " with: " << ex.what();
        }
    });
}

void WebsocketConnection::onClosed(std::function<void()> callback) {
    ws_->onClosed(std::move(callback));
}

void WebsocketConnection::open(const std::string& url) {
    ws_->open(url);
}

void WebsocketConnection::onError(std::function<void(const std::string& error)> callback) {
    ws_->onError(std::move(callback));
}

void WebsocketConnection::checkAlive() {
    const nlohmann::json ping = {{"type", "PING"}};
    const size_t timeout = 1000;  // 1s
    const size_t currentPongs = pongCounter_;
    auto msg = ping.dump();
    NABTO_SIGNALING_LOGD << "WS sending PING: " << msg;
    ws_->send(msg);
    auto self = shared_from_this();
    timer_ = timerFactory_->createTimer();
    timer_->setTimeout(timeout, [currentPongs, self]() {
        if (self->pongCounter_ == currentPongs) {
            NABTO_SIGNALING_LOGE << "Alive Timeout";
            self->ws_->close();
        }
    });
}

void WebsocketConnection::handlePong() {
    NABTO_SIGNALING_LOGD << "WS handle PONG";
    pongCounter_++;
}

SignalingMessageType WebsocketConnection::parseWsMsgType(const std::string& str) {
    if (str == "MESSAGE") {
        return SignalingMessageType::MESSAGE;
    }
    if (str == "ERROR") {
        return SignalingMessageType::ERROR;
    }
    if (str == "PEER_OFFLINE") {
        return SignalingMessageType::PEER_OFFLINE;
    }
    if (str == "PEER_CONNECTED") {
        return SignalingMessageType::PEER_CONNECTED;
    }
    if (str == "PING") {
        return SignalingMessageType::PING;
    }
    if (str == "PONG") {
        return SignalingMessageType::PONG;
    }
    throw std::invalid_argument("Invalid message type: " + str);
}
}  // namespace signaling
}  // namespace nabto
