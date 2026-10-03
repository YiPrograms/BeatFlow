#pragma once

#include "beatflow/quest/AndroidKeystore.hpp"
#include "beatflow/services/OAuthClient.hpp"

#include <filesystem>

namespace beatflow::quest {

class QuestCredentialStore final : public CredentialStore {
  public:
    explicit QuestCredentialStore(std::filesystem::path root);

    Outcome<OAuthClientCredentials> loadClientCredentials() override;
    Outcome<std::optional<OAuthTokens>> loadTokens() override;
    Outcome<bool> saveTokens(const OAuthTokens& tokens) override;
    Outcome<bool> clearTokens() override;

    Outcome<bool> clearAll();
    [[nodiscard]] std::filesystem::path importPath() const;

  private:
    Outcome<std::string> readEncrypted(const std::filesystem::path& path);
    Outcome<bool> writeEncrypted(const std::filesystem::path& path, const std::string& value);

    std::filesystem::path root_;
    AndroidKeystore keystore_;
};

} // namespace beatflow::quest
