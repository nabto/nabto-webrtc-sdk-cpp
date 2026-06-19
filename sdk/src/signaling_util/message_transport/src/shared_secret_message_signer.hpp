#pragma once

#include <nlohmann/json.hpp>
#include "message_signer.hpp"

namespace nabto {
namespace webrtc {
namespace util {

class SharedSecretMessageSigner {
 public:
  static MessageSignerPtr create(const std::string& secret, const std::string& secretId);
  static std::string getKeyId(const nlohmann::json& message);
};

}  // namespace util
}  // namespace webrtc
}  // namespace nabto
