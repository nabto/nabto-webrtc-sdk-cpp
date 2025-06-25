#pragma once

#include "message_signer.hpp"
#include <jwt-cpp/traits/nlohmann-json/traits.h>

#include <nabto/webrtc/device.hpp>
#include <nabto/webrtc/util/logging.hpp>
#include <nabto/webrtc/util/uuid.hpp>

#include <nlohmann/json.hpp>

#include <memory>

namespace nabto {
namespace webrtc {
namespace util {

class SharedSecretMessageSigner : public MessageSigner {
 public:
  static MessageSignerPtr create(std::string& secret, std::string& secretId) {
    auto sig = std::make_shared<SharedSecretMessageSigner>(secret, secretId);
    return sig;
  }

  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
  SharedSecretMessageSigner(std::string& secret, std::string& secretId)
      : secret_(secret), secretId_(secretId) {
    myNonce_ = generate_uuid_v4();
  }

  static std::string getKeyId(const nlohmann::json& message) {
    NPLOGD << "signaling signer handle msg" << message.dump();
    try {
      auto jwt = message.at("jwt").get<std::string>();
      auto decoded = jwt::decode<jwt::traits::nlohmann_json>(jwt);
      auto kidClaim = decoded.get_header_claim("kid");
      auto keyId = kidClaim.as_string();
      return keyId;
    } catch (std::exception& ex) {
      NPLOGE << "Failed to get key ID from JWT: " << ex.what();
      return "";
    }
  }

  nlohmann::json signMessage(const nlohmann::json& msg) override {
    if (nextMessageSignSeq_ != 0 && remoteNonce_.empty()) {
      NPLOGE << "Tried to sign message with seq: " << nextMessageSignSeq_
             << " and an empty remote nonce";
      throw std::runtime_error("Invalid State");
    }
    NPLOGD << "Signing message with seq: " << nextMessageSignSeq_
           << " and remoteNonce: " << remoteNonce_;

    const uint32_t seq = nextMessageSignSeq_;
    nextMessageSignSeq_++;

    auto jwt = jwt::create<jwt::traits::nlohmann_json>()
                   .set_payload_claim("message", msg)
                   .set_payload_claim("messageSeq", seq)
                   .set_payload_claim("signerNonce", myNonce_);

    if (!secretId_.empty()) {
      jwt.set_key_id(secretId_);
    }

    if (!remoteNonce_.empty()) {
      jwt.set_payload_claim("verifierNonce", remoteNonce_);
    }

    auto token = jwt.sign(jwt::algorithm::hs256(secret_));

    nlohmann::json message = {{"type", "JWT"}, {"jwt", token}};

    return message;
  }

  nlohmann::json verifyMessage(const nlohmann::json& msg) override {
    NPLOGD << "signaling signer handle msg" << msg.dump();
    try {
      auto jwt = msg.at("jwt").get<std::string>();
      auto decoded = jwt::decode(jwt);

      // TODO(tk): handle optional key ID and find secret based on key ID if it
      // exists auto keyId = decoded.get_key_id();
      auto verifier =
          jwt::verify().allow_algorithm(jwt::algorithm::hs256(secret_));
      verifier.verify(decoded);
      const std::string& pl = decoded.get_payload();
      auto data = nlohmann::json::parse(pl);
      NPLOGD << "DATA: " << data.dump();
      const uint32_t claimedSeq = data.at("messageSeq").get<uint32_t>();
      if (claimedSeq != nextMessageVerifySeq_) {
        throw VerificationError();
      }
      const std::string signerNonce = data.at("signerNonce").get<std::string>();
      if (claimedSeq == 0) {
        remoteNonce_ = signerNonce;
      } else {
        if (signerNonce != remoteNonce_) {
          throw VerificationError();
        }
        const std::string verifyNonce =
            data.at("verifierNonce").get<std::string>();
        if (verifyNonce != myNonce_) {
          throw VerificationError();
        }
      }
      nextMessageVerifySeq_++;
      auto message = data.at("message");
      return message;
    } catch (std::exception& ex) {
      NPLOGE << "Failed to validate JWT: " << ex.what();
      throw VerificationError();
    }
  }

 private:
  std::string secret_;
  std::string secretId_;

  uint32_t nextMessageSignSeq_ = 0;
  uint32_t nextMessageVerifySeq_ = 0;
  std::string myNonce_;
  std::string remoteNonce_;
};

}  // namespace util
}  // namespace webrtc
}  // namespace nabto
