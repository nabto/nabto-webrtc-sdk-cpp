#include <iostream>
#include <string_view>

#include <plog/Appenders/ColorConsoleAppender.h>
#include <plog/Formatters/TxtFormatter.h>
#include <toml++/toml.hpp>

#include <libdatachannel_websocket/rtc_websocket_wrapper.hpp>

#include <nabto/webrtc/device.hpp>
#include <nabto/webrtc/util/logging.hpp>
#include <nabto/webrtc/util/curl_async.hpp>
#include <nabto/webrtc/util/message_transport.hpp>
#include <nabto/webrtc/util/std_timer.hpp>
#include <nabto/webrtc/util/token_generator.hpp>

#include "connection.hpp"
#include "h264_handler.hpp"

using namespace std::literals;
using std::string_view;
using std::string;

struct DeviceSettings {
    string_view product_id;
    string_view device_id;
    string_view private_key;
};

struct AuthSettings {
    bool central_auth;
    string_view shared_secret;
};

enum class VideoCodec
{
    H264,
    H265,
    VP8
};

enum class AudioCodec
{
    OPUS
};

struct StreamSettings {
    VideoCodec video_codec;
    AudioCodec audio_codec;
};

struct Settings {
    DeviceSettings device;
    AuthSettings auth;
    StreamSettings stream;
};

static bool load_options(toml::v3::ex::parse_result& config, Settings& settings) {
    bool valid = true;

    string device_required[] = {
        "product_id",
        "device_id",
        "private_key"
    };

    for (const string& key : device_required) {
        if (!config["device"][key].is_string()) {
            NPLOGE << key << " is not correctly specified in config.toml";
            valid = false;
        }
    }

    settings.device.product_id = config["device"]["product_id"].value_or("sv");
    settings.device.device_id = config["device"]["device_id"].value_or(""sv);
    settings.device.private_key = config["device"]["private_key"].value_or(""sv);

    settings.auth.shared_secret = config["auth"]["shared_secret"].value_or(""sv);
    settings.auth.central_auth = config["auth"]["central_auth"].value_or(false);

    return valid;
}

static void handle_new_channel(
    nabto::webrtc::SignalingDevicePtr device,
    Settings& settings, 
    nabto::webrtc::SignalingChannelPtr channel,
    nabto::example::H264TrackHandlerPtr trackHandler,
    bool authorized) {

    if (settings.auth.central_auth) {
        if (!authorized) {
            auto errorMessage = "Rejecting connectiong as central authorization is required";
            auto error = nabto::webrtc::SignalingError{
                nabto::webrtc::SignalingErrorCode::ACCESS_DENIED,
                errorMessage
            };

            NPLOGE << errorMessage;
            channel->sendError(error);
            channel->close();
            return;
        }
    } else if (settings.auth.shared_secret.empty()) {
        auto errorMessage =
        "This device is not configured to use either shared secrets or central auth. "
        "Connection will be closed.";
        auto error = nabto::webrtc::SignalingError{
            nabto::webrtc::SignalingErrorCode::ACCESS_DENIED,
            errorMessage
        };

        channel->sendError(error);
        channel->close();
        return;
    }

    nabto::webrtc::util::MessageTransportPtr transport;
    if (settings.auth.shared_secret.empty()) {
        transport = nabto::webrtc::util::MessageTransportFactory::createNoneTransport(device, channel);
    } else {
        transport = nabto::webrtc::util::MessageTransportFactory::createSharedSecretTransport(
            device, channel, [&settings](const string key_id) -> string {
                return string{settings.auth.shared_secret};
            }
        );
    }

    // @TODO: Create connection.
    auto web_conn = nabto::example::WebrtcConnection::create(device, channel, transport, trackHandler);
}

int main() {

    // Init logging
    {
        auto log_level = plog::Severity::verbose;
        static plog::ColorConsoleAppender<plog::TxtFormatter> console_appender;
        nabto::webrtc::util::initLogger(log_level, &console_appender);
        plog::init<nabto::webrtc::SIGNALING_LOGGER_INSTANCE_ID>(log_level, &console_appender);
    }

    // Parse options
    auto config = toml::parse_file("config.toml");
    Settings settings = {};
    load_options(config, settings);

    string product_id{settings.device.product_id};
    string device_id{settings.device.device_id};
    string private_key{settings.device.private_key};

    nabto::webrtc::SignalingTokenGeneratorPtr jwt_ptr = nabto::webrtc::util::NabtoTokenGenerator::create(
        product_id,
        device_id,
        private_key
    );

    NPLOGI << "Connecting to device: " << settings.device.device_id;
    auto http = nabto::webrtc::util::CurlHttpClient::create(std::nullopt);
    auto ws = nabto::example::RtcWebsocketWrapper::create(std::nullopt);
    auto tf = nabto::webrtc::util::StdTimerFactory::create();
    auto trackHandler = nabto::example::H264TrackHandler::create(nullptr);

    nabto::webrtc::SignalingDeviceConfig signalingConfig = {
        device_id,
        product_id,
        jwt_ptr,
        "",
        ws,
        http,
        tf
    };

    auto device = nabto::webrtc::SignalingDeviceFactory::create(signalingConfig);

    device->addNewChannelListener(
        [device, trackHandler, &settings]
        (nabto::webrtc::SignalingChannelPtr channel, bool authorized) {
            handle_new_channel(device, settings, channel, trackHandler, authorized);
        }
    );
    
    device->start();

    int n;
    std::cin >> n;
    return 0;
}