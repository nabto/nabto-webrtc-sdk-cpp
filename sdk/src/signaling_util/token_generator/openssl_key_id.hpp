#include <openssl/encoder.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace nabto {
namespace example {

struct EVP_MD_CTXFree {
  void operator()(EVP_MD_CTX *ctx) { EVP_MD_CTX_free(ctx); }
};

struct EVP_PKEYFree {
  void operator()(EVP_PKEY *ctx) { EVP_PKEY_free(ctx); }
};

struct BIOFree {
  void operator()(BIO *ctx) { BIO_free(ctx); }
};

typedef std::unique_ptr<EVP_MD_CTX, EVP_MD_CTXFree> EVP_MD_CTXPtr;
typedef std::unique_ptr<EVP_PKEY, EVP_PKEYFree> EVP_PKEYPtr;
typedef std::unique_ptr<BIO, BIOFree> BIOPtr;

static bool sha256hex(const std::vector<uint8_t> &data, std::string &hash) {
  std::vector<uint8_t> outputBuffer =
      std::vector<uint8_t>(EVP_MD_size(EVP_sha256()));
  unsigned int outputLength;
  EVP_MD_CTXPtr ctx = EVP_MD_CTXPtr(EVP_MD_CTX_new());
  const EVP_MD *md = EVP_sha256();  // Select SHA256 algorithm

  if (ctx == NULL) {
    return false;
  }

  // Initialize, process, and finalize the hash computation
  if (!EVP_DigestInit_ex(ctx.get(), md, NULL)) {
    return false;
  }
  if (!EVP_DigestUpdate(ctx.get(), data.data(), data.size())) {
    return false;
  }
  if (!EVP_DigestFinal_ex(ctx.get(), outputBuffer.data(), &outputLength)) {
    return false;
  }

  int i = 0;
  std::stringstream hexOutput;
  for (i = 0; i < outputBuffer.size(); i++) {
    hexOutput << std::setfill('0') << std::setw(2) << std::hex
              << static_cast<int>(outputBuffer[i]);
  }

  hash = hexOutput.str();
  return true;
}

static bool getSpkiFromPrivateKey(const std::string &privateKey,
                                  std::vector<uint8_t> &spki) {
  BIOPtr bio(BIO_new_mem_buf((void *)privateKey.c_str(),
                             -1));  // -1 = null-terminated string
  if (!bio) {
    fprintf(stderr, "Failed to create BIO\n");
    return false;
  }

  // Read the private key from the BIO
  EVP_PKEYPtr pkey(PEM_read_bio_PrivateKey(bio.get(), NULL, NULL, NULL));
  if (!pkey) {
    fprintf(stderr, "Error reading private key: ");
    ERR_print_errors_fp(stderr);
    return false;
  }

  int dataLen = 0;

  unsigned char *encodedData = nullptr;
  dataLen = i2d_PUBKEY(pkey.get(), &encodedData);
  if (dataLen <= 0) {
    return false;
  }
  spki = std::vector<uint8_t>(encodedData, encodedData + dataLen);
  return true;
}

static bool getKeyIdFromPrivateKey(const std::string &privateKey,
                                   std::string &keyId) {
  std::vector<uint8_t> spki;
  if (!getSpkiFromPrivateKey(privateKey, spki)) {
    return false;
  }

  std::string hash;
  if (!sha256hex(spki, hash)) {
    return false;
  }

  keyId = "device:" + hash;
  return true;
}
}  // namespace example
}  // namespace nabto
