#pragma once

#include "message_signer.hpp"
#include <jwt-cpp/jwt.h>

#include <nabto/webrtc/device.hpp>
#include <nabto/webrtc/util/logging.hpp>

#include <nlohmann/json.hpp>

#include <memory>

namespace nabto {
namespace util {

class NoneMessageSigner : public MessageSigner {
 public:
  static MessageSignerPtr create() {
    auto signer = std::make_shared<NoneMessageSigner>();
    return signer;
  }

  NoneMessageSigner() {}

  nlohmann::json signMessage(const nlohmann::json& msg) override {
    nlohmann::json data = {{"type", "NONE"}, {"message", msg}};
    return data;
  }

  nlohmann::json verifyMessage(const nlohmann::json& msg) override {
    NPLOGD << "signaling signer handle msg" << msg;
    try {
      auto data = msg["message"];
      return data;
    } catch (std::exception& ex) {
      NPLOGE << "Failed to parse JSON: " << ex.what();
      throw VerificationError();
    }
  }
};

}  // namespace util
}  // namespace nabto
