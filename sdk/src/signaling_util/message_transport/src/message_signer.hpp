#pragma once

#include <nlohmann/json.hpp>

#include <memory>
#include <string>

namespace nabto {
namespace util {

class MessageSigner;
typedef std::shared_ptr<MessageSigner> MessageSignerPtr;

class VerificationError : public std::exception {
 public:
  VerificationError() {}
  virtual ~VerificationError() {}
};

class MessageSigner {
 public:
  virtual ~MessageSigner() {}
  /**
   * Verify a message and return the resulting message
   */
  virtual nlohmann::json verifyMessage(const nlohmann::json& message) = 0;

  /**
   * Sign a message and return the signed message
   */
  virtual nlohmann::json signMessage(const nlohmann::json& message) = 0;
};
}  // namespace util
}  // namespace nabto
