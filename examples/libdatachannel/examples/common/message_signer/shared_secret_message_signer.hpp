#pragma once

#include <jwt-cpp/traits/nlohmann-json/traits.h>

#include <nabto/webrtc/util/uuid.hpp>
// #include <jwt-cpp/jwt.h>

#include <memory>
#include <nabto/webrtc/device.hpp>
#include <nabto/webrtc/util/logging.hpp>
#include <nlohmann/json.hpp>

#include "message_signer.hpp"

namespace nabto {
namespace example {

class SharedSecretMessageSigner : public MessageSigner {
 public:
  static MessageSignerPtr create(std::string& secret, std::string& secretId) {
    auto sig = std::make_shared<SharedSecretMessageSigner>(secret, secretId);
    return sig;
  }

  SharedSecretMessageSigner(std::string& secret, std::string& secretId)
      : secret_(secret), secretId_(secretId) {
    myNonce_ = generate_uuid_v4();
  }

  nlohmann::json signMessage(const nlohmann::json& msg) override {
    if (nextMessageSignSeq_ != 0 && remoteNonce_.empty()) {
      NPLOGE << "Tried to sign message with seq: " << nextMessageSignSeq_
             << " and an empty remote nonce";
      throw std::runtime_error("Invalid State");
    }
    NPLOGD << "Signing message with seq: " << nextMessageSignSeq_
           << " and remoteNonce: " << remoteNonce_;

    uint32_t seq = nextMessageSignSeq_;
    nextMessageSignSeq_++;

    auto jwt = jwt::create<jwt::traits::nlohmann_json>()
                   .set_payload_claim("message", msg)
                   .set_payload_claim("messageSeq", seq)
                   .set_payload_claim("signerNonce", myNonce_)
                   .set_key_id(secretId_);

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
      auto jwt = msg["jwt"].get<std::string>();
      auto decoded = jwt::decode(jwt);

      // TODO: handle optional key ID and find secret based on key ID if it
      // exists auto keyId = decoded.get_key_id();
      auto verifier =
          jwt::verify().allow_algorithm(jwt::algorithm::hs256(secret_));
      verifier.verify(decoded);
      std::string pl = decoded.get_payload();
      auto data = nlohmann::json::parse(pl);
      NPLOGD << "DATA: " << data.dump();
      uint32_t claimedSeq = data["messageSeq"].get<uint32_t>();
      if (claimedSeq != nextMessageVerifySeq_) {
        throw VerificationError();
      }
      std::string signerNonce = data["signerNonce"].get<std::string>();
      if (claimedSeq == 0) {
        remoteNonce_ = signerNonce;
      } else {
        if (signerNonce != remoteNonce_) {
          throw VerificationError();
        }
        std::string verifyNonce = data["verifierNonce"].get<std::string>();
        if (verifyNonce != myNonce_) {
          throw VerificationError();
        }
      }
      nextMessageVerifySeq_++;
      auto message = data["message"];
      return message;
    } catch (std::exception& ex) {
      NPLOGE << "Failed to validate JWT: " << ex.what();
      std::string err = "VERIFICATION_ERROR";
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

}  // namespace example
}  // namespace nabto
