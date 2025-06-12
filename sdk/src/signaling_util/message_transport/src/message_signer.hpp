#pragma once

#include <nlohmann/json.hpp>

#include <memory>
#include <string>

namespace nabto {
namespace util {

class MessageSigner;
using MessageSignerPtr = std::shared_ptr<MessageSigner>;

class VerificationError : public std::exception {
 public:
  ~VerificationError() override = default;
  VerificationError() = default;
  VerificationError(const VerificationError&) = delete;
  VerificationError& operator=(const VerificationError&) = delete;
  VerificationError(VerificationError&&) = delete;
  VerificationError& operator=(VerificationError&&) = delete;
};

class MessageSigner {
 public:
  virtual ~MessageSigner() = default;
  MessageSigner() = default;
  MessageSigner(const MessageSigner&) = delete;
  MessageSigner& operator=(const MessageSigner&) = delete;
  MessageSigner(MessageSigner&&) = delete;
  MessageSigner& operator=(MessageSigner&&) = delete;

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
