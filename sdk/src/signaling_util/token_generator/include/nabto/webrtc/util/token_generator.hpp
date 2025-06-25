#pragma once

#include "../../../../src/openssl_key_id.hpp"
#include <jwt-cpp/jwt.h>

#include <nabto/webrtc/device.hpp>
#include <nabto/webrtc/util/uuid.hpp>

#include <chrono>
#include <memory>
#include <string>

namespace nabto {
namespace util {

/**
 * Thalhammer/jwt-cpp based SignalingTokenGenerator implementation to use for
 * generating JWTs for the SDK.
 */
class NabtoTokenGenerator
    : public nabto::webrtc::SignalingTokenGenerator,
      public std::enable_shared_from_this<NabtoTokenGenerator> {
 public:
  /**
   * Create a NabtoTokenGenerator.
   *
   * @param productId The Product ID to make tokens for.
   * @param deviceId The Device ID to make tokens for.
   * @param privateKey The private key (in PEM format) to use to sign the
   * tokens.
   * @return SignalingTokenGeneratorPtr pointing to the created object.
   */
  static nabto::webrtc::SignalingTokenGeneratorPtr create(
      std::string productId, std::string deviceId, std::string privateKey) {
    return std::make_shared<NabtoTokenGenerator>(productId, deviceId,
                                                 privateKey);
  }

  NabtoTokenGenerator(std::string productId, std::string deviceId,
                      std::string privateKey)
      : productId_(productId), deviceId_(deviceId), privateKey_(privateKey) {}

  bool generateToken(std::string& token) {
    std::string keyId;

    if (!getKeyIdFromPrivateKey(privateKey_, keyId)) {
      return false;
    }

    std::string resource = "urn:nabto:webrtc:" + productId_ + ":" + deviceId_;
    std::string scope = "device:connect turn";
    auto t = jwt::create()
                 .set_type("JWT")
                 .set_expires_at(std::chrono::system_clock::now() +
                                 std::chrono::seconds{3600})
                 .set_issued_at(std::chrono::system_clock::now())
                 .set_key_id(keyId)
                 .set_payload_claim("resource", jwt::claim(resource))
                 .set_payload_claim("scope", jwt::claim(scope))
                 .sign(jwt::algorithm::es256(publicKey_, privateKey_));
    token = t;
    return true;
  }

 private:
  std::string productId_;
  std::string deviceId_;
  std::string keyId_;
  std::string privateKey_;
  std::string publicKey_;
};

}  // namespace util
}  // namespace nabto
