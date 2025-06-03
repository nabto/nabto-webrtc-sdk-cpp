#include "openssl_key_id.hpp"

#include <gtest/gtest.h>

std::string privateKeyPem = R"(-----BEGIN PRIVATE KEY-----
MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQg+oH56dcdavHeaJhO
kroaroSWQLA/A+6sCQRb8g+Ip4yhRANCAATc3dMAfNPk6dmWOLoYdOLwsuC6OQ4x
1vOzzk4iv+0GYsurToJkZ7FohDBPiup+FNLWyaiUnXgdWay/vLjH2P1k
-----END PRIVATE KEY-----)";

TEST(token_generator, create_key_id) {
  std::string keyId;
  bool status = nabto::example::getKeyIdFromPrivateKey(privateKeyPem, keyId);
  ASSERT_EQ(status, true);
  ASSERT_EQ(keyId,
            "device:"
            "d253a3df618f08d76696ddc66fdc35de5d75ed12e1908503b1575e005e79a516");
}

TEST(token_generator, sha256hash) {
  std::string hash;
  std::vector<uint8_t> toHash = std::vector<uint8_t>();
  ASSERT_TRUE(nabto::example::sha256hex(toHash, hash));
}
