#pragma once

#include <jwt-cpp/jwt.h>

#include <memory>
#include <nlohmann/json.hpp>

#include <logging/logging.hpp>
#include "message_signer.hpp"
#include "nabto/signaling/signaling.hpp"

namespace nabto {
namespace example {

class SharedSecretMessageSigner : public MessageSigner {
   public:
    static MessageSignerPtr create(std::string& secret, std::string& secretId) {
        auto sig = std::make_shared<SharedSecretMessageSigner>(secret, secretId);
        return sig;
    }

    SharedSecretMessageSigner(std::string& secret, std::string& secretId) : secret_(secret), secretId_(secretId) {
    }

    std::string signMessage(const std::string& msg) override {
        auto seq = signSeq_;
        signSeq_++;
        auto jwt = jwt::create()
                       .set_payload_claim("message", jwt::claim(msg))
                       .set_payload_claim("messageSeq", jwt::claim(picojson::value(seq)))
                       .set_key_id(secretId_);

        auto token = jwt.sign(jwt::algorithm::hs256(secret_));
        return token;
    }

    std::string verifyMessage(const std::string& msg) override {
        NPLOGD << "signaling signer handle msg" << msg;
        try {
            auto decoded = jwt::decode(msg);
            auto keyId = decoded.get_key_id();
            // TODO: find secret based on key ID
            auto verifier = jwt::verify()
                                .allow_algorithm(jwt::algorithm::hs256(secret_));
            verifier.verify(decoded);
            std::string pl = decoded.get_payload();
            auto data = nlohmann::json::parse(pl);
            auto message = data["message"].get<std::string>();
            // TODO verify message sequence number
            return message;
        } catch (std::exception& ex) {
            NPLOGE << "Failed to validate JWT: " << ex.what();
            std::string err = "VERIFICATION_ERROR";
            throw VerificationError();
        }
    }

   private:
    /**
     * Incrementing number on signed messages
     */
    double signSeq_ = 0;

    std::string secret_;
    std::string secretId_;
};

}  // namespace example
}  // namespace nabto
