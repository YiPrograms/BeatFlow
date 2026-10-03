#pragma once

#include "beatflow/core/Error.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace beatflow::quest {

struct EncryptedSecret {
    std::vector<std::uint8_t> initializationVector;
    std::vector<std::uint8_t> ciphertext;
};

// AES-256-GCM with a non-exportable key owned by AndroidKeyStore. Only the IV
// and authenticated ciphertext are written to storage.
class AndroidKeystore {
  public:
    explicit AndroidKeystore(std::string alias = "BeatFlow.Secrets.v1");

    Outcome<EncryptedSecret> encrypt(std::span<const std::uint8_t> plaintext) const;
    Outcome<std::vector<std::uint8_t>> decrypt(const EncryptedSecret& secret) const;

  private:
    std::string alias_;
};

} // namespace beatflow::quest
