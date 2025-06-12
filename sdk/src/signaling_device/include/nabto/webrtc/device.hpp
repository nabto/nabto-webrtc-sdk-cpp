#pragma once

#include <plog/Log.h>

#include <nlohmann/json.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace nabto {
namespace signaling {

constexpr int SIGNALING_LOGGER_INSTANCE_ID = 822;

class SignalingDevice;
using SignalingDevicePtr = std::shared_ptr<SignalingDevice>;

class SignalingChannel;
using SignalingChannelPtr = std::shared_ptr<SignalingChannel>;

class SignalingWebsocket;
using SignalingWebsocketPtr = std::shared_ptr<SignalingWebsocket>;

class SignalingHttpClient;
using SignalingHttpClientPtr = std::shared_ptr<SignalingHttpClient>;

class SignalingTimer;
using SignalingTimerPtr = std::shared_ptr<SignalingTimer>;

class SignalingTimerFactory;
using SignalingTimerFactoryPtr = std::shared_ptr<SignalingTimerFactory>;

class SignalingTokenGenerator;
using SignalingTokenGeneratorPtr = std::shared_ptr<SignalingTokenGenerator>;

/**
 * HTTP Request abstraction used by the SDK.
 */
class SignalingHttpRequest {
 public:
  /**
   * Method of the request. "GET" | "POST"
   */
  std::string method;

  /**
   * URL to send the request to
   */
  std::string url;
  /**
   * List of HTTP headers as key-value pairs.
   */
  std::vector<std::pair<std::string, std::string> > headers;

  /**
   * The body of the HTTP request.
   */
  std::string body;
};

/**
 * HTTP Response abstraction used by the SDK.
 */
class
    SignalingHttpResponse {  // NOLINT(cppcoreguidelines-pro-type-member-init,hicpp-member-init)
 public:
  /**
   * The HTTP status code of the response.
   */
  int statusCode;

  /**
   * List of HTTP headers as key-value pairs.
   */
  std::vector<std::pair<std::string, std::string> > headers;

  /**
   * The body of the HTTP request.
   */
  std::string body;
};

enum class SignalingErrorCode : std::uint8_t {
  DECODE_ERROR,
  VERIFICATION_ERROR,
  CHANNEL_CLOSED,
  CHANNEL_NOT_FOUND,
  NO_MORE_CHANNELS,
  ACCESS_DENIED,
  INTERNAL_ERROR
};

/**
 * Class defining the error format used by the SDK
 */
class SignalingError {
 public:
  /**
   * Construct a SignalingError
   *
   * @param code An error code
   * @param message An error message string
   */
  SignalingError(SignalingErrorCode code, std::string message);

  /**
   * Construct a SignalingError
   *
   * @param code An error code string
   * @param message An error message string
   */
  SignalingError(std::string code, std::string message);

  /**
   * Get the error code
   * @returns The error code
   */
  std::string errorCode() const { return errorCode_; };

  /**
   * Get the error message
   * @returns The error message
   */
  std::string errorMessage() const { return errorMessage_; };

  static std::string errorCodeToString(SignalingErrorCode code);

 private:
  std::string errorCode_;
  std::string errorMessage_;
};

/**
 * Callback to be invoked when a HTTP request has resolved
 *
 * @param response if the response is non null the request succeeded, else it
 * failed.
 */
using HttpResponseCallback =
    std::function<void(std::unique_ptr<SignalingHttpResponse> response)>;

/**
 * HTTP client interface used by the SDK when it needs to make a HTTP request
 */
class SignalingHttpClient {
 public:
  virtual ~SignalingHttpClient() = default;
  SignalingHttpClient() = default;
  SignalingHttpClient(const SignalingHttpClient&) = delete;
  SignalingHttpClient& operator=(const SignalingHttpClient&) = delete;
  SignalingHttpClient(SignalingHttpClient&&) = delete;
  SignalingHttpClient& operator=(SignalingHttpClient&&) = delete;
  /**
   * Send a HTTP request
   *
   * If the function returns true, the callback must be invoked when the request
   * is resolved no matter the resolved state.
   *
   * If the callback is invoked with statusCode = 0, it signals that the request
   * failed without a response. When statusCode = 0, the response string can be
   * empty or be an error message.
   *
   * @param method "GET" | "POST"
   * @param url The URL to send the request to
   * @param request The request to send
   * @param callback callback to be invoked when the request is resolved.
   * @return true if the request was accepted.
   */
  virtual bool sendRequest(const SignalingHttpRequest& request,
                           HttpResponseCallback callback) = 0;
};

/**
 * Websocket abstraction to use by the SDK
 */
class SignalingWebsocket {
 public:
  virtual ~SignalingWebsocket() = default;
  SignalingWebsocket() = default;
  SignalingWebsocket(const SignalingWebsocket&) = delete;
  SignalingWebsocket& operator=(const SignalingWebsocket&) = delete;
  SignalingWebsocket(SignalingWebsocket&&) = delete;
  SignalingWebsocket& operator=(SignalingWebsocket&&) = delete;
  /**
   * Send a string of data on the websocket
   *
   * @param data the data to send
   * @return true if the data was sent
   */
  virtual bool send(const std::string& data) = 0;

  /**
   * Close the websocket
   */
  virtual void close() = 0;

  /**
   * set callback to be invoked when the Websocket connection is open
   *
   * @param callback the callback to set
   */
  virtual void onOpen(std::function<void()> callback) = 0;

  /**
   * set callback to be invoked when a message is received on the websocket
   *
   * @param callback the callback to set
   */
  virtual void onMessage(
      std::function<void(const std::string& message)> callback) = 0;

  /**
   * set callback to be invoked when the websocket connection is closed
   *
   * @param callback the callback to set
   */
  virtual void onClosed(std::function<void()> callback) = 0;

  /**
   * set callback to be invoked if an error occurs on the websocket connection
   *
   * @param callback the callback to set
   */
  virtual void onError(
      std::function<void(const std::string& error)> callback) = 0;

  /**
   * Open a websocket connection
   *
   * @param url the URL to connect to
   */
  virtual void open(const std::string& url) = 0;
};

/**
 * Timer factory the SDK can use to create timers
 */
class SignalingTimerFactory {
 public:
  virtual ~SignalingTimerFactory() = default;
  SignalingTimerFactory() = default;
  SignalingTimerFactory(const SignalingTimerFactory&) = delete;
  SignalingTimerFactory& operator=(const SignalingTimerFactory&) = delete;
  SignalingTimerFactory(SignalingTimerFactory&&) = delete;
  SignalingTimerFactory& operator=(SignalingTimerFactory&&) = delete;
  /**
   * Create a timer for the SDK
   *
   * @return signaling timer pointer
   */
  virtual SignalingTimerPtr createTimer() = 0;
};

/**
 * Timer abstraction the SDK can create with the SignalingTimerFactory
 */
class SignalingTimer {
 public:
  virtual ~SignalingTimer() = default;
  SignalingTimer() = default;
  SignalingTimer(const SignalingTimer&) = delete;
  SignalingTimer& operator=(const SignalingTimer&) = delete;
  SignalingTimer(SignalingTimer&&) = delete;
  SignalingTimer& operator=(SignalingTimer&&) = delete;
  /**
   * Set a timeout in ms at which the callback sould be invoked
   *
   * @param timeoutMs The timeout in milliseconds
   * @param callback The callback to be invoked once the timeout has passed
   */
  virtual void setTimeout(uint32_t timeoutMs,
                          std::function<void()> callback) = 0;
};

/**
 * Token generator to create JWT tokens suitable for connecting to the Nabto
 * Backend.
 */
class SignalingTokenGenerator {
 public:
  virtual ~SignalingTokenGenerator() = default;
  SignalingTokenGenerator() = default;
  SignalingTokenGenerator(const SignalingTokenGenerator&) = delete;
  SignalingTokenGenerator& operator=(const SignalingTokenGenerator&) = delete;
  SignalingTokenGenerator(SignalingTokenGenerator&&) = delete;
  SignalingTokenGenerator& operator=(SignalingTokenGenerator&&) = delete;

  /**
   * Create a token for connecting to the backend
   *
   * @param token The string object to write the token to
   * @return true iff the token was generated
   */
  virtual bool generateToken(std::string& token) = 0;
};

/**
 * Events the signaling device can emit
 */
enum class SignalingDeviceState : std::uint8_t {
  NEW,
  CONNECTING,
  CONNECTED,
  WAIT_RETRY,
  FAILED,
  CLOSED,
};

/**
 * Convert a SignalingDeviceState enum to a string
 * @param state The state to convert
 * @return The string representation of the state
 */
std::string signalingDeviceStateToString(SignalingDeviceState state);

/**
 * Events a Signaling Channel can emit.
 *
 *  - NEW: The channel was just created.
 *  - ONLINE: The SDK received an indication that the client is online.
 *  - OFFLINE: The SDK tried to send a message to the client but the client was
 * offline.
 *  - FAILED: The channel received a error, which is fatal in the protocol.
 *  - CLOSED: The channel was closed by the application
 */
enum class SignalingChannelState : std::uint8_t {
  NEW,
  ONLINE,
  OFFLINE,
  FAILED,
  CLOSED
};

/**
 * Convert a SignalingChannelState enum to a string
 * @param state The state to convert
 * @return The string representation of the state
 */
std::string signalingChannelStateToString(SignalingChannelState state);

/**
 * struct representing an ICE server returned by the Nabto Backend
 */
struct IceServer {
  /**
   * username will be the empty string if the server is a STUN server, and a
   * username if it is a TURN server
   */
  std::string username;
  /**
   * credential will be the empty string if the server is a STUN server, and a
   * credential if it is a TURN server
   */
  std::string credential;
  /**
   * List of URLs for the ICE server. If the server is a TURN server, the
   * credentials will be valid for all URLs in the list.
   */
  std::vector<std::string> urls;
};

/**
 * Callback function definition when a new signaling channel is available.
 */
using NewSignalingChannelHandler =
    std::function<void(SignalingChannelPtr conn, bool authorized)>;

/**
 * Callback function definition when a new signaling message is available.
 */
using SignalingMessageHandler = std::function<void(const nlohmann::json& msg)>;

/**
 * Callback function definition when the device state changes.
 */
using SignalingDeviceStateHandler =
    std::function<void(SignalingDeviceState state)>;

/**
 * Callback function definition when the signaling channel state changes.
 */
using SignalingChannelStateHandler =
    std::function<void(SignalingChannelState state)>;

/**
 * Callback function definition when the signaling connection reconnects.
 */
using SignalingReconnectHandler = std::function<void(void)>;

/**
 * Callback function definition when a signaling error occurs.
 */
using SignalingErrorHandler = std::function<void(const SignalingError& error)>;

/**
 * Callback function definition when a new ICE servers response is received.
 */
using IceServersResponse =
    std::function<void(const std::vector<struct IceServer>&)>;

/**
 * Configuration used when constructing a Signaler.
 *
 * deviceId and productId are values configured in the Nabto Backend
 * tokenProvider is a function the Signaler calls when it requires a token
 * allowing the Signaler to attach to the Nabto Backend signalingUrl is the URL
 * for the Nabto Backend eg. https://signalingpoc.dev.nabto.com wsImpl is an
 * instance of the Websocket abstraction defined above httpCli is an instance of
 * the HTTP client abstraction defined above timerFactory is an instance of the
 * Timer factory abstraction defined above
 */
struct SignalingDeviceConfig {
  /**
   * Device ID from the Nabto Cloud Console
   */
  std::string deviceId;

  /**
   * Product ID from the Nabto Cloud Console
   */
  std::string productId;

  /**
   * Token provider implementation the SDK can use to generate JWTs used to
   * connect to the Nabto Signaling Service
   */
  SignalingTokenGeneratorPtr tokenProvider;

  /**
   * Optional signaling URL. If left empty, the SDK will construct a default
   * URL.
   */
  std::string signalingUrl;

  /**
   * WebSocket implementation the SDK can use to send and receive signaling
   * messages.
   */
  SignalingWebsocketPtr wsImpl;

  /**
   * HTTP client implementation the SDK can use to send HTTP requests and
   * receive responses.
   */
  SignalingHttpClientPtr httpCli;

  /**
   * Timer factory implementation the SDK can use to create timers.
   */
  SignalingTimerFactoryPtr timerFactory;
};

/**
 * Factory class used to construct a Signaling Device.
 */
class SignalingDeviceFactory {
 public:
  /**
   * Create a new SignalingDevice.
   *
   * @param conf Configuration to use for the signaling device
   * @returns Smart pointer to the created SignalingDevice
   */
  static SignalingDevicePtr create(const SignalingDeviceConfig& conf);
};

/**
 * Signaling Device Class
 */
class SignalingDevice {
 public:
  virtual ~SignalingDevice() = default;
  SignalingDevice() = default;
  SignalingDevice(const SignalingDevice&) = delete;
  SignalingDevice& operator=(const SignalingDevice&) = delete;
  SignalingDevice(SignalingDevice&&) = delete;
  SignalingDevice& operator=(SignalingDevice&&) = delete;

  /**
   * Connect the Signaler to the Nabto Backend
   */
  virtual void start() = 0;

  /**
   * Close the Signaler
   */
  virtual void close() = 0;

  /**
   * Trigger the Signaler to validate that its Websocket connection is alive. If
   * the connection is still alive, nothing happens. Otherwise, the Signaler
   * will reconnect to the backend and trigger a Signaling Event.
   */
  virtual void checkAlive() = 0;

  /**
   * Request ICE servers from the Nabto Backend
   *
   * @param callback callback to be invoked when the request is resolved
   */
  virtual void requestIceServers(IceServersResponse callback) = 0;

  // TODO(tk): switch from set...Handler to add...Handler and add
  // remove...Handler

  /**
   * Set handler to be called when a new Client connects
   *
   * @param handler Handler to be called when a client connects
   */
  virtual void setNewChannelHandler(NewSignalingChannelHandler handler) = 0;

  /**
   * Set a handler to be invoked when the device state changes.
   *
   * @param handler the handler to set
   */
  virtual void setStateChangeHandler(SignalingDeviceStateHandler handler) = 0;

  /**
   * Set a handler to be invoked when the connection is reconnected.
   *
   * @param handler the handler to set
   */
  virtual void setReconnectHandler(SignalingReconnectHandler handler) = 0;
};

/**
 * Signaling Channel Class
 */
class SignalingChannel {
 public:
  virtual ~SignalingChannel() = default;
  SignalingChannel() = default;
  SignalingChannel(const SignalingChannel&) = delete;
  SignalingChannel& operator=(const SignalingChannel&) = delete;
  SignalingChannel(SignalingChannel&&) = delete;
  SignalingChannel& operator=(SignalingChannel&&) = delete;

  /**
   * Set a handler to be invoked whenever a message is available on the
   * connection
   *
   * @param handler the handler to set
   */
  virtual void setMessageHandler(SignalingMessageHandler handler) = 0;

  /**
   * Set a handler to be invoked when the channel state changes.
   *
   * @param handler the handler to set
   */
  virtual void setStateChangeHandler(SignalingChannelStateHandler handler) = 0;

  /**
   * Set a handler to be invoked if an error occurs on the connection
   *
   * @param handler the handler to set
   */
  virtual void setErrorHandler(SignalingErrorHandler handler) = 0;

  /**
   * Send a signaling message to the client
   *
   * @param message The message to send
   */
  virtual void sendMessage(const nlohmann::json& message) = 0;

  /**
   * Send a signaling error to the client
   *
   * @param error The error to send
   */
  virtual void sendError(const SignalingError& error) = 0;

  /**
   * Close the Signaling channel
   */
  virtual void close() = 0;

  /**
   * Get the channel ID of the channel.
   *
   * This can be used to correlate events between the device and the client
   * implementations.
   *
   * @return The channel ID string
   */
  virtual std::string getChannelId() = 0;
};

}  // namespace signaling
}  // namespace nabto
