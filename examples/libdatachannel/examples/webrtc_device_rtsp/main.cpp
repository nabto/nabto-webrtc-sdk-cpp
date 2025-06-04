#include <plog/Appenders/ColorConsoleAppender.h>
#include <plog/Formatters/TxtFormatter.h>

#include <curl_http_client/curl_async.hpp>
#include <cxxopts.hpp>
#include <fstream>
#include <iostream>
#include <libdatachannel_websocket/rtc_websocket_wrapper.hpp>
#include <message_signer/none_message_signer.hpp>
#include <message_signer/shared_secret_message_signer.hpp>
#include <nabto/webrtc/device.hpp>
#include <nabto/webrtc/util/logging.hpp>
#include <std_timer/std_timer.hpp>
#include <token_generator/token_generator.hpp>
#include <webrtc_connection/webrtc_connection.hpp>

#include "h264_opus_rtsp_handler.hpp"

struct options {
  std::string signalingUrl;
  std::string deviceId;
  std::string productId;
  plog::Severity logLevel;
  std::string privateKey;
  std::string sharedSecret;
  std::string sharedSecretId;
  bool centralAuthorization;
  std::string rtspUrl;
};

bool parse_options(int argc, char** argv, struct options& opts);
struct options defaultOptions();
std::string readKeyFile(std::string path);

int main(int argc, char** argv) {
  auto opts = defaultOptions();
  if (!parse_options(argc, argv, opts)) {
    return 0;
  }

  static plog::ColorConsoleAppender<plog::TxtFormatter> consoleAppender;
  nabto::example::initLogger(opts.logLevel, &consoleAppender);

  // init logging for NabtoSignaling::core
  plog::init<nabto::signaling::SIGNALING_LOGGER_INSTANCE_ID>(opts.logLevel,
                                                             &consoleAppender);

  NPLOGI << "Connecting to device: " << opts.deviceId;

  nabto::example::NabtoJwtPtr jwtPtr = nabto::example::NabtoJwt::create(
      opts.productId, opts.deviceId, opts.privateKey);

  auto http = nabto::example::CurlHttpClient::create();
  auto ws = nabto::example::RtcWebsocketWrapper::create();
  auto tf = nabto::example::StdTimerFactory::create();
  auto trackHandler = nabto::example::H264TrackHandler::create(opts.rtspUrl);

  nabto::signaling::SignalingDeviceConfig conf = {
      opts.deviceId, opts.productId, jwtPtr, opts.signalingUrl, ws, http, tf};

  auto device = nabto::signaling::SignalingDeviceFactory::create(conf);
  device->setNewChannelHandler(
      [device, trackHandler, &opts /*, &conns*/](
          nabto::signaling::SignalingChannelPtr channel, bool authorized) {
        // Handle authorization
        if (opts.centralAuthorization) {
          if (!authorized) {
            NPLOGE
                << "Rejecting connection as central authorization is required";
            auto authorizationErrorMessage =
                "Rejecting connection as central authorization is required";
            NPLOGE << authorizationErrorMessage;
            channel->sendError(nabto::signaling::SignalingError(
                nabto::signaling::SignalingErrorCode::ACCESS_DENIED,
                authorizationErrorMessage));
            channel->close();
            return;
          }
        } else if (opts.sharedSecret.empty()) {
          auto authorizationErrorMessage =
              "Not accepting connection as it is neither central authorization "
              "or shared secret message signing is used";
          NPLOGE << authorizationErrorMessage;
          channel->sendError(nabto::signaling::SignalingError(
              nabto::signaling::SignalingErrorCode::ACCESS_DENIED,
              authorizationErrorMessage));
          channel->close();
          return;
        }
        nabto::example::MessageSignerPtr signer;
        if (opts.sharedSecret.empty()) {
          signer = nabto::example::NoneMessageSigner::create();
        } else {
          signer = nabto::example::SharedSecretMessageSigner::create(
              opts.sharedSecret, opts.sharedSecretId);
        }
        auto webConn = nabto::example::WebrtcConnection::create(
            device, channel, signer, trackHandler);
        // conns.push_back(webConn);
      });
  device->start();

  int n;
  std::cin >> n;
}

bool parse_options(int argc, char** argv, struct options& opts) {
  try {
    cxxopts::Options options(argv[0], "Webrtc Device");
    options.add_options()("d,deviceid", "Device ID for device",
                          cxxopts::value<std::string>())(
        "p,productid", "Product ID for device", cxxopts::value<std::string>())(
        "log-level", "Optional. The log level (error|info|trace)",
        cxxopts::value<std::string>())(
        "k,private-key-file", "Optional. path to pem private key file to use",
        cxxopts::value<std::string>())(
        "s,signaling-url", "Optional. URL for the signaling service",
        cxxopts::value<std::string>())(
        "secret",
        "Optional. Shared secret used to sign and validate signaling messages",
        cxxopts::value<std::string>())("central-authorization",
                                       "Require central authorization")(
        "r,rtsp-url", "URL for the RTSP server", cxxopts::value<std::string>())(
        "h,help", "Shows this help text");
    auto result = options.parse(argc, argv);

    if (result.count("help")) {
      std::cout << options.help({"", "Group"}) << std::endl;
      return false;
    }

    if (result.count("deviceid") && result.count("productid")) {
      opts.deviceId = result["deviceid"].as<std::string>();
      opts.productId = result["productid"].as<std::string>();
    } else {
      std::cout << "device ID required" << std::endl;
      std::cout << options.help({"", "Group"}) << std::endl;
      return false;
    }

    if (result.count("rtsp-url")) {
      opts.rtspUrl = result["rtsp-url"].as<std::string>();
    } else {
      std::cout << "RTSP URL required" << std::endl;
      std::cout << options.help({"", "Group"}) << std::endl;
      return false;
    }

    if (result.count("log-level")) {
      auto lvl = result["log-level"].as<std::string>();
      if (lvl == "error") {
        opts.logLevel = plog::error;
      } else if (lvl == "info") {
        opts.logLevel = plog::info;
      } else if (lvl == "trace") {
        opts.logLevel = plog::debug;
      } else {
        std::cout << "Invalid log level provided: " << lvl << " using default"
                  << std::endl;
      }
    }

    if (result.count("private-key-file")) {
      opts.privateKey =
          readKeyFile(result["private-key-file"].as<std::string>());
    }

    if (result.count("signaling-url")) {
      opts.signalingUrl = result["signaling-url"].as<std::string>();
    }

    if (result.count("secret")) {
      opts.sharedSecret = result["secret"].as<std::string>();
    }
    if (result.count("central-authorization")) {
      opts.centralAuthorization = true;
    }

    if (opts.sharedSecret.empty() && opts.centralAuthorization == false) {
      std::cout << "Error atleast one of the options --secret or "
                   "--central-authorization needs to be enabled"
                << std::endl;
      return false;
    }

  } catch (const cxxopts::exceptions::exception& e) {
    std::cout << "Error parsing options: " << e.what() << std::endl;
    return false;
  }
  return true;
}

std::string readKeyFile(std::string path) {
  std::stringstream ss;
  std::ifstream readFile(path);

  std::string line;
  while (getline(readFile, line)) {
    // Output the text from the file
    ss << line << "\r\n";
  }

  // Close the file
  readFile.close();
  return ss.str();
}

struct options defaultOptions() {
  struct options opts = {"", "", "", plog::debug, "", "", "default", false};
  return opts;
}
