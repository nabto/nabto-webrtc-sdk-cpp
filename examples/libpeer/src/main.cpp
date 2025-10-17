#include <libwebsockets.h>
#include <plog/Appenders/ColorConsoleAppender.h>
#include <plog/Formatters/TxtFormatter.h>
#include <stdio.h>

#include <nabto/webrtc/device.hpp>
#include <nabto/webrtc/util/logging.hpp>
#include <nabto/webrtc/util/std_timer.hpp>
#include <nabto/webrtc/util/token_generator.hpp>
#include <optional>
#include <string>

#include "deviceconf.hpp"
#include "libpeer_connection.hpp"
#include "lws_http_client.hpp"
#include "lws_websocket.hpp"

using std::string;

int main() {
  static plog::ColorConsoleAppender<plog::TxtFormatter> consoleAppender;
  nabto::webrtc::util::initLogger(plog::Severity::debug, &consoleAppender);

  // init logging for Nabtonabto::webrtc::core
  plog::init<nabto::webrtc::SIGNALING_LOGGER_INSTANCE_ID>(plog::Severity::debug,
                                                          &consoleAppender);

  nabto::webrtc::SignalingTokenGeneratorPtr jwtPtr =
      nabto::webrtc::util::NabtoTokenGenerator::create(productId, deviceId,
                                                       privateKey);

  auto http = nabto::example::LwsHttpClient::create();
  auto ws = nabto::example::LwsWebsocket::create();
  auto tf = nabto::webrtc::util::StdTimerFactory::create();

  nabto::webrtc::SignalingDeviceConfig conf = {deviceId, productId, jwtPtr, "",
                                               ws,       http,      tf};

  auto device = nabto::webrtc::SignalingDeviceFactory::create(conf);

  device->addNewChannelListener(
      [device](nabto::webrtc::SignalingChannelPtr channel, bool authorized) {
        NPLOGI << "New channel!";
        auto transport = nabto::webrtc::util::MessageTransportFactory::
            createSharedSecretTransport(
                device, channel, [](const std::string keyId) -> std::string {
                  return sharedSecret;
                });

        auto conn = nabto::example::WebrtcConnection::create(device, channel,
                                                             transport);
      });

  device->start();

  int n;
  std::cin >> n;
  return 0;
}
