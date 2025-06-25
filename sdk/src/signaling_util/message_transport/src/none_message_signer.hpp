#pragma once

#include "message_signer.hpp"
#include <jwt-cpp/jwt.h>

#include <nabto/webrtc/device.hpp>
#include <nabto/webrtc/util/logging.hpp>

#include <nlohmann/json.hpp>

#include <memory>

namespace nabto {
namespace webrtc {
namespace util {

class NoneMessageSigner : public MessageSigner {
 public:
  static MessageSignerPtr create() {
    auto signer = std::make_shared<NoneMessageSigner>();
    return signer;
  }

  NoneMessageSigner() = default;

  nlohmann::json signMessage(const nlohmann::json& msg) override {
    nlohmann::json data = {{"type", "NONE"}, {"message", msg}};
    return data;
  }

  nlohmann::json verifyMessage(const nlohmann::json& msg) override {
    NPLOGD << "signaling signer handle msg" << msg.dump();
    try {
      auto type = msg.at("type").get<std::string>();
      if (type != "NONE") {
        throw VerificationError();
      }
      auto data = msg.at("message");
      return data;
    } catch (nlohmann::json::exception& ex) {
      NPLOGE << "Failed to parse JSON: " << ex.what();
      throw DecodeError();
    }
  }
};

}  // namespace util
}  // namespace webrtc
}  // namespace nabto
