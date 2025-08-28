
#include "signaling_device_impl.hpp"

#include "logging.hpp"
#include "signaling_channel_impl.hpp"
#include "signaling_impl.hpp"
#include "websocket_connection.hpp"

#include <nabto/webrtc/device.hpp>

#include <nlohmann/json_fwd.hpp>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace {
nlohmann::json signalingErrorToJson(const nabto::webrtc::SignalingError& err) {
  nlohmann::json error = {{"code", err.errorCode()},
                          {"message", err.errorMessage()}};
  return error;
}

nabto::webrtc::SignalingError signalingErrorFromJson(
    const nlohmann::json& err) {
  std::string msg;
  if (err.contains("message")) {
    msg = err.at("message").get<std::string>();
  }

  return {err.at("code").get<std::string>(), msg};
}

}  // namespace

namespace nabto {
namespace webrtc {

SignalingDeviceImplPtr SignalingDeviceImpl::create(
    const SignalingDeviceConfig& conf) {
  return std::make_shared<SignalingDeviceImpl>(conf);
}

SignalingDeviceImpl::SignalingDeviceImpl(const SignalingDeviceConfig& conf)
    : wsImpl_(conf.wsImpl),
      httpCli_(conf.httpCli),
      deviceId_(conf.deviceId),
      productId_(conf.productId),
      tokenProvider_(conf.tokenProvider),
      httpHost_(conf.signalingUrl),
      ws_(WebsocketConnection::create(wsImpl_, conf.timerFactory)),
      timerFactory_(conf.timerFactory) {
  if (httpHost_.empty()) {
    httpHost_ = "https://" + productId_ + DEFAULT_SIGNALING_DOMAIN;
  }
}

void SignalingDeviceImpl::start() {
  mutex_.lock();
  NABTO_SIGNALING_LOGI << "Signaling Device started in version: " << version();
  if (state_ == SignalingDeviceState::NEW) {
    mutex_.unlock();
    doConnect();
  } else {
    NABTO_SIGNALING_LOGE << "Connect called from invalid state: "
                         << signalingDeviceStateToString(state_);
    mutex_.unlock();
  }
}

void SignalingDeviceImpl::doConnect() {
  {
    const std::lock_guard<std::mutex> lock(mutex_);
    if (state_ == SignalingDeviceState::CLOSED ||
        state_ == SignalingDeviceState::FAILED) {
      NABTO_SIGNALING_LOGD << "doConnect called from closed state, stopping";
      return;
    }
  }
  changeState(SignalingDeviceState::CONNECTING);
  mutex_.lock();

  const std::string method = "POST";
  const std::string url = httpHost_ + "/v1/device/connect";

  SignalingHttpRequest req;
  req.method = method;
  req.url = url;
  std::string token;
  if (!tokenProvider_->generateToken(token)) {
    NABTO_SIGNALING_LOGE
        << "Cannot create an access token using the provided token provider.";
    mutex_.unlock();
    changeState(SignalingDeviceState::FAILED);
    return;
  }
  req.headers.emplace_back("Authorization", "Bearer " + token);
  req.headers.emplace_back("Content-Type", "application/json");
  req.body =
      nlohmann::json({{"deviceId", deviceId_}, {"productId", productId_}})
          .dump();

  auto self = shared_from_this();
  httpCli_->sendRequest(
      req, [self](const std::unique_ptr<SignalingHttpResponse>& response) {
        const int httpOkStartRange = 200;
        const int httpOkEndRange = 299;
        if (response == nullptr) {
          NABTO_SIGNALING_LOGE << "HTTP request failed ";
          self->waitReconnect();
        } else if (response->statusCode < httpOkStartRange ||
                   response->statusCode > httpOkEndRange) {
          NABTO_SIGNALING_LOGE << "HTTP request failed with status: "
                               << response->statusCode;
          self->waitReconnect();
        } else {
          self->parseAttachResponse(response->body);
          self->connectWs();
        }
      });
  mutex_.unlock();
}

void SignalingDeviceImpl::websocketSendMessage(const std::string& channelId,
                                               const nlohmann::json& message) {
  const std::lock_guard<std::mutex> lock(mutex_);
  if (state_ == SignalingDeviceState::CONNECTED) {
    const nlohmann::json msg = {
        {"type", "MESSAGE"}, {"channelId", channelId}, {"message", message}};
    NABTO_SIGNALING_LOGD << "Sending WS msg: " << msg.dump();
    ws_->send(msg.dump());
  } else {
    NABTO_SIGNALING_LOGD << "Tried to send message but WS not connected";
  }
}

void SignalingDeviceImpl::websocketSendError(const std::string& channelId,
                                             const SignalingError& error) {
  const std::lock_guard<std::mutex> lock(mutex_);
  if (state_ == SignalingDeviceState::CONNECTED) {
    nlohmann::json const msg = {{"type", "ERROR"},
                                {"channelId", channelId},
                                {"error", signalingErrorToJson(error)}};
    NABTO_SIGNALING_LOGD << "Sending WS Error: " << msg.dump();
    ws_->send(msg.dump());
  } else {
    NABTO_SIGNALING_LOGD << "Tried to send error but WS not connected";
  }
}

void SignalingDeviceImpl::close() {
  std::map<std::string, nabto::webrtc::SignalingChannelImplPtr> chans;
  {
    {
      const std::lock_guard<std::mutex> lock(mutex_);
      chans = channels_;
      closed_ = true;
    }
    changeState(SignalingDeviceState::CLOSED);
  }
  for (auto const& channel : chans) {
    channel.second->wsClosed();
    channelClosed(channel.first);
  }
  nabto::webrtc::WebsocketConnectionPtr ws;
  {
    const std::lock_guard<std::mutex> lock(mutex_);
    channels_.clear();

    ws = ws_;
  }
  if (ws) {
    ws->close();
  }
  if (!ws) {
    deinit();
  }
}

void SignalingDeviceImpl::deinit() {
  SignalingTimerPtr timer = nullptr;
  {
    const std::lock_guard<std::mutex> lock(mutex_);
    timer = timer_;
    timer_ = nullptr;
  }
  if (timer) {
    timer->cancel();
  }
  const std::lock_guard<std::mutex> lock(mutex_);
  chanHandlers_.clear();
  stateHandlers_.clear();
  reconnHandlers_.clear();
}

void SignalingDeviceImpl::connectWs() {
  const std::lock_guard<std::mutex> lock(mutex_);
  auto self = shared_from_this();
  ws_ = WebsocketConnection::create(wsImpl_, timerFactory_);
  ws_->onOpen([self]() {
    NABTO_SIGNALING_LOGI << "WebSocket open";
    std::map<ReconnectListenerId, SignalingReconnectHandler> reconnHandlers;
    {
      const std::lock_guard<std::mutex> lock(self->mutex_);
      if (self->firstConnect_) {
        self->firstConnect_ = false;
      } else {
        reconnHandlers = self->reconnHandlers_;
      }
    }
    // if firstConnect the map will be empty
    for (const auto& [id, handler] : reconnHandlers) {
      handler();
    }
    self->changeState(SignalingDeviceState::CONNECTED);
  });

  ws_->onMessage([self](SignalingMessageType type, nlohmann::json& message) {
    self->handleWsMessage(type, message);
  });

  ws_->onClosed([self]() {
    NABTO_SIGNALING_LOGD << "Websocket closed";
    if (self->state_ != SignalingDeviceState::CLOSED &&
        self->state_ != SignalingDeviceState::FAILED) {
      self->waitReconnect();
    } else {
      self->deinit();
    }
  });

  ws_->onError([self](const std::string& error) {
    NABTO_SIGNALING_LOGD << "Websocket error: " << error;
    if (self->state_ != SignalingDeviceState::CLOSED &&
        self->state_ != SignalingDeviceState::FAILED) {
      self->waitReconnect();
    } else {
      self->deinit();
    }
  });

  ws_->open(wsUrl_);
}

void SignalingDeviceImpl::handleWsMessage(SignalingMessageType type,
                                          const nlohmann::json& message) {
  NABTO_SIGNALING_LOGD << "handleWsMessage of type: " << type
                       << " message: " << message.dump();
  mutex_.lock();
  if (type == SignalingMessageType::PING) {
    sendPong();
    mutex_.unlock();
    return;
  }
  SignalingChannelImplPtr chan = nullptr;
  try {
    const std::string connId = message.at("channelId").get<std::string>();
    try {
      chan = channels_.at(connId);
      if (type == SignalingMessageType::PEER_OFFLINE) {
        mutex_.unlock();
        chan->peerOffline();
        return;
      }
      if (type == SignalingMessageType::PEER_CONNECTED) {
        mutex_.unlock();
        chan->peerConnected();
        return;
      }
      if (type == SignalingMessageType::ERROR) {
        mutex_.unlock();
        chan->handleError(signalingErrorFromJson(message.at("error")));
        return;
      }
    } catch (std::exception& exception) {
      NABTO_SIGNALING_LOGD << "Got websocket channel for unknown channel ID: "
                           << connId;
      // if message type is MESSAGE, we will create a connection below,
      // otherwise we ignore the message
    }

    if (type == SignalingMessageType::MESSAGE) {
      bool authorized = false;
      if (message.contains("authorized")) {
        authorized = message.at("authorized").get<bool>();
      } else {
        NABTO_SIGNALING_LOGD
            << "authorized bit not contained in incoming message"
            << message.dump();
      }
      const auto& msg = message.at("message");
      if (chan == nullptr) {
        // channel is unknown
        if (!SignalingChannelImpl::isInitialMessage(msg)) {
          NABTO_SIGNALING_LOGE
              << "Got an message for an unknown channel, but the message is "
                 "not an initial message. Discarding the message";
          mutex_.unlock();
          websocketSendError(
              connId, SignalingError(SignalingErrorCode::CHANNEL_NOT_FOUND,
                                     "Got a message for a signaling channel "
                                     "which does not exist."));
          return;
        }
        auto self = shared_from_this();
        chan = SignalingChannelImpl::create(self, connId);
        channels_.insert(std::make_pair(connId, chan));

        if (chanHandlers_.empty()) {
          mutex_.unlock();
          websocketSendError(
              connId,
              SignalingError(
                  SignalingErrorCode::INTERNAL_ERROR,
                  "No NewChannelHandler was set, dropping the channel."));
          return;
        }
        auto chanHandlers = chanHandlers_;
        mutex_.unlock();

        for (const auto& [id, handler] : chanHandlers) {
          handler(chan, authorized);
        }
      }
      mutex_.unlock();
      chan->handleMessage(msg);
      return;
    }

    if (chan == nullptr) {
      NABTO_SIGNALING_LOGD << "Got unhandled message from unknown channel ID. "
                              "SignalingMessageType: "
                           << type << " " << message.dump();
    } else {
      NABTO_SIGNALING_LOGE << "Got unhandled message. SignalingMessageType: "
                           << type << " " << message.dump();
    }
  } catch (std::exception& exception) {
    NABTO_SIGNALING_LOGE << "Invalid channel ID in websocket message: "
                         << message.dump() << " error: " << exception.what();
  }
  mutex_.unlock();
}

void SignalingDeviceImpl::sendPong() {
  const nlohmann::json pong = {{"type", "PONG"}};
  NABTO_SIGNALING_LOGI << "Sending WS msg: " << pong.dump();
  ws_->send(pong.dump());
}

void SignalingDeviceImpl::parseAttachResponse(const std::string& response) {
  try {
    const std::lock_guard<std::mutex> lock(mutex_);
    auto root = nlohmann::json::parse(response);
    wsUrl_ = root.at("signalingUrl").get<std::string>();

  } catch (std::exception& exception) {
    NABTO_SIGNALING_LOGE << "Failed parse attach response: "
                         << exception.what();
  }
}

void SignalingDeviceImpl::checkAlive() {
  const std::lock_guard<std::mutex> lock(mutex_);
  if (state_ != SignalingDeviceState::CLOSED &&
      state_ != SignalingDeviceState::FAILED) {
    ws_->checkAlive();
  } else {
    NABTO_SIGNALING_LOGE << "checkAlive called from invalid state: "
                         << signalingDeviceStateToString(state_);
  }
}

void SignalingDeviceImpl::waitReconnect() {
  {
    const std::lock_guard<std::mutex> lock(mutex_);
    if (state_ == SignalingDeviceState::CLOSED ||
        state_ == SignalingDeviceState::FAILED) {
      NABTO_SIGNALING_LOGD << "Ignoring waitReconnect call from closed state";
      return;
    }
    if (state_ == SignalingDeviceState::WAIT_RETRY) {
      NABTO_SIGNALING_LOGD
          << "Ignoring waitReconnect call from wait_retry state";
      return;
    }
  }
  changeState(SignalingDeviceState::WAIT_RETRY);

  {
    const std::lock_guard<std::mutex> lock(mutex_);
    ws_ = nullptr;
    const uint32_t oneMinuteInMs = 60000;
    const uint32_t msPrSecond = 1000;
    const size_t exponentialBackoffCap = 6;
    uint32_t reconnectWait = oneMinuteInMs;
    if (reconnectCounter_ < exponentialBackoffCap) {
      reconnectWait =
          msPrSecond * (static_cast<uint32_t>(1) << reconnectCounter_);
    }
    reconnectCounter_++;
    timer_ = timerFactory_->createTimer();
    auto self = shared_from_this();
    timer_->setTimeout(reconnectWait, [self]() { self->doConnect(); });
  }
}

void SignalingDeviceImpl::requestIceServers(IceServersResponse callback) {
  const std::lock_guard<std::mutex> lock(mutex_);
  const std::string method = "POST";
  const std::string url = httpHost_ + "/v1/ice-servers";
  std::string token;
  if (!tokenProvider_->generateToken(token)) {
    NABTO_SIGNALING_LOGE
        << "Cannot create an access token using the provided token provider.";
  }
  SignalingHttpRequest req;
  req.method = method;
  req.url = url;
  req.headers.emplace_back("Authorization", "Bearer " + token);
  req.headers.emplace_back("Content-Type", "application/json");
  req.body =
      nlohmann::json({{"deviceId", deviceId_}, {"productId", productId_}})
          .dump();

  auto self = shared_from_this();
  httpCli_->sendRequest(
      req,
      [self, callback](const std::unique_ptr<SignalingHttpResponse>& response) {
        if (response == nullptr) {
          callback({});
        } else {
          const int httpOkStartRange = 200;
          const int httpOkEndRange = 299;
          if (response->statusCode >= httpOkStartRange &&
              response->statusCode <= httpOkEndRange) {
            callback(SignalingDeviceImpl::parseIceServers(response->body));
          } else {
            callback({});
          }
        }
      });
}

void SignalingDeviceImpl::channelClosed(const std::string& channelId) {
  websocketSendError(
      channelId,
      SignalingError(SignalingErrorCode::CHANNEL_CLOSED,
                     "The signaling channel has been closed in the device."));
  const std::lock_guard<std::mutex> lock(mutex_);
  channels_.erase(channelId);
}

std::vector<struct IceServer> SignalingDeviceImpl::parseIceServers(
    const std::string& data) {
  std::vector<struct IceServer> result;
  try {
    auto ice = nlohmann::json::parse(data);
    auto servers = ice.at("iceServers").get<nlohmann::json::array_t>();
    for (auto server : servers) {
      std::string username;
      if (server.contains("username")) {
        username = server.at("username").get<std::string>();
      }
      std::string cred;
      if (server.contains("credential")) {
        cred = server.at("credential").get<std::string>();
      }

      std::vector<std::string> iceUrls;
      auto urls = server.at("urls").get<nlohmann::json::array_t>();
      for (auto const& url : urls) {
        iceUrls.push_back(url.get<std::string>());
      }
      const struct IceServer iceServer = {username, cred, iceUrls};

      result.push_back(iceServer);
    }
  } catch (std::exception& exception) {
    NABTO_SIGNALING_LOGE << "Failed to parse ICE servers with: "
                         << exception.what() << " Parsing data: " << data;
  }
  return result;
}

void SignalingDeviceImpl::changeState(SignalingDeviceState state) {
  state_ = state;
  for (const auto& [id, handler] : stateHandlers_) {
    handler(state);
  }
}

}  // namespace webrtc
}  // namespace nabto
