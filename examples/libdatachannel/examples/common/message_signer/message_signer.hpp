#pragma once

#include <memory>
#include <string>

namespace nabto {
namespace example {

class MessageSigner;
typedef std::shared_ptr<MessageSigner> MessageSignerPtr;

class VerificationError : public std::exception {
   public:
    VerificationError() {
    }
    virtual ~VerificationError() {}
};

class MessageSigner {
   public:
    virtual ~MessageSigner() {}
    /**
     * Verify a message and return the resulting message
     */
    virtual std::string verifyMessage(const std::string& message) = 0;

    /**
     * Sign a message and return the signed message
     */
    virtual std::string signMessage(const std::string& message) = 0;
};
}  // namespace signaling
}  // namespace nabto
