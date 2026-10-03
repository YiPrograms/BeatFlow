#include "beatflow/quest/QuestCredentialStore.hpp"

#include <nlohmann/json.hpp>

#include <array>
#include <cstring>
#include <fstream>
#include <sys/stat.h>

namespace beatflow::quest {
namespace {

constexpr std::array<char, 8> kMagic{'B', 'F', 'S', 'E', 'C', '0', '1', '\0'};

ServiceError storageError(std::string message) {
    return {ErrorCode::Storage, std::move(message), false, std::nullopt};
}

void append32(std::vector<std::uint8_t>& output, std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8) {
        output.push_back(static_cast<std::uint8_t>(value >> shift));
    }
}

std::uint32_t read32(const std::vector<std::uint8_t>& input, std::size_t offset) {
    return static_cast<std::uint32_t>(input[offset]) | static_cast<std::uint32_t>(input[offset + 1]) << 8U |
           static_cast<std::uint32_t>(input[offset + 2]) << 16U |
           static_cast<std::uint32_t>(input[offset + 3]) << 24U;
}

} // namespace

QuestCredentialStore::QuestCredentialStore(std::filesystem::path root) : root_(std::move(root)) {}

Outcome<OAuthClientCredentials> QuestCredentialStore::loadClientCredentials() {
    const auto encryptedPath = root_ / "oauth_client.enc";
    auto stored = readEncrypted(encryptedPath);
    if (!stored && stored.error().code == ErrorCode::NotFound) {
        std::ifstream import(importPath(), std::ios::binary);
        if (!import) {
            return Outcome<OAuthClientCredentials>::failure(
                {ErrorCode::Configuration,
                 "Personalized For You needs your own Google OAuth client. Copy oauth_client.json to "
                 "/sdcard/ModData/com.beatgames.beatsaber/Mods/BeatFlow/. Anonymous Up Next does not "
                 "need an account.",
                 false, std::nullopt});
        }
        std::string plaintext((std::istreambuf_iterator<char>(import)), std::istreambuf_iterator<char>());
        auto imported = writeEncrypted(encryptedPath, plaintext);
        if (!imported) {
            return Outcome<OAuthClientCredentials>::failure(imported.error());
        }
        std::error_code ignored;
        std::filesystem::remove(importPath(), ignored);
        stored = Outcome<std::string>::success(std::move(plaintext));
    }
    if (!stored) {
        return Outcome<OAuthClientCredentials>::failure(stored.error());
    }
    try {
        const auto json = nlohmann::json::parse(stored.value());
        OAuthClientCredentials credentials{json.at("client_id").get<std::string>(),
                                           json.at("client_secret").get<std::string>()};
        if (credentials.clientId.empty() || credentials.clientSecret.empty()) {
            throw std::runtime_error("empty client credential");
        }
        return Outcome<OAuthClientCredentials>::success(std::move(credentials));
    } catch (const std::exception& exception) {
        return Outcome<OAuthClientCredentials>::failure(
            {ErrorCode::Configuration,
             std::string("The OAuth client credentials are invalid: ") + exception.what(), false,
             std::nullopt});
    }
}

Outcome<std::optional<OAuthTokens>> QuestCredentialStore::loadTokens() {
    auto stored = readEncrypted(root_ / "oauth_tokens.enc");
    if (!stored && stored.error().code == ErrorCode::NotFound) {
        return Outcome<std::optional<OAuthTokens>>::success(std::nullopt);
    }
    if (!stored) {
        return Outcome<std::optional<OAuthTokens>>::failure(stored.error());
    }
    try {
        const auto json = nlohmann::json::parse(stored.value());
        OAuthTokens tokens{json.at("access_token").get<std::string>(),
                           json.at("refresh_token").get<std::string>(), json.value("token_type", "Bearer"),
                           json.at("expires_at").get<std::int64_t>()};
        return Outcome<std::optional<OAuthTokens>>::success(std::move(tokens));
    } catch (const std::exception&) {
        std::error_code ignored;
        std::filesystem::remove(root_ / "oauth_tokens.enc", ignored);
        return Outcome<std::optional<OAuthTokens>>::failure(
            storageError("The encrypted YouTube Music session was corrupt and has been removed."));
    }
}

Outcome<bool> QuestCredentialStore::saveTokens(const OAuthTokens& tokens) {
    nlohmann::json json{{"access_token", tokens.accessToken},
                        {"refresh_token", tokens.refreshToken},
                        {"token_type", tokens.tokenType},
                        {"expires_at", tokens.expiresAtEpochSeconds}};
    return writeEncrypted(root_ / "oauth_tokens.enc", json.dump());
}

Outcome<bool> QuestCredentialStore::clearTokens() {
    std::error_code error;
    std::filesystem::remove(root_ / "oauth_tokens.enc", error);
    if (error) {
        return Outcome<bool>::failure(storageError("Could not remove the YouTube Music session."));
    }
    return Outcome<bool>::success(true);
}

Outcome<bool> QuestCredentialStore::clearAll() {
    auto tokens = clearTokens();
    if (!tokens) {
        return tokens;
    }
    std::error_code error;
    std::filesystem::remove(root_ / "oauth_client.enc", error);
    if (error) {
        return Outcome<bool>::failure(storageError("Could not remove the imported OAuth credentials."));
    }
    std::filesystem::remove(importPath(), error);
    if (error) {
        return Outcome<bool>::failure(storageError("Could not remove the pending OAuth credentials file."));
    }
    return Outcome<bool>::success(true);
}

std::filesystem::path QuestCredentialStore::importPath() const {
    return root_ / "oauth_client.json";
}

Outcome<std::string> QuestCredentialStore::readEncrypted(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return Outcome<std::string>::failure(
            {ErrorCode::NotFound, "Encrypted account data does not exist.", false, std::nullopt});
    }
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(input)),
                                    std::istreambuf_iterator<char>());
    if (bytes.size() < kMagic.size() + 4 || std::memcmp(bytes.data(), kMagic.data(), kMagic.size()) != 0) {
        return Outcome<std::string>::failure(storageError("Encrypted account data has an invalid header."));
    }
    const auto ivSize = read32(bytes, kMagic.size());
    const auto payloadOffset = kMagic.size() + 4 + ivSize;
    if (ivSize == 0 || payloadOffset >= bytes.size()) {
        return Outcome<std::string>::failure(storageError("Encrypted account data is incomplete."));
    }
    EncryptedSecret secret;
    secret.initializationVector.assign(bytes.begin() + static_cast<std::ptrdiff_t>(kMagic.size() + 4),
                                       bytes.begin() + static_cast<std::ptrdiff_t>(payloadOffset));
    secret.ciphertext.assign(bytes.begin() + static_cast<std::ptrdiff_t>(payloadOffset), bytes.end());
    auto plaintext = keystore_.decrypt(secret);
    if (!plaintext) {
        return Outcome<std::string>::failure(plaintext.error());
    }
    return Outcome<std::string>::success(
        std::string(reinterpret_cast<const char*>(plaintext.value().data()), plaintext.value().size()));
}

Outcome<bool> QuestCredentialStore::writeEncrypted(const std::filesystem::path& path,
                                                   const std::string& value) {
    const auto* raw = reinterpret_cast<const std::uint8_t*>(value.data());
    auto encrypted = keystore_.encrypt(std::span<const std::uint8_t>(raw, value.size()));
    if (!encrypted) {
        return Outcome<bool>::failure(encrypted.error());
    }
    std::vector<std::uint8_t> bytes(kMagic.begin(), kMagic.end());
    append32(bytes, static_cast<std::uint32_t>(encrypted.value().initializationVector.size()));
    bytes.insert(bytes.end(), encrypted.value().initializationVector.begin(),
                 encrypted.value().initializationVector.end());
    bytes.insert(bytes.end(), encrypted.value().ciphertext.begin(), encrypted.value().ciphertext.end());

    std::error_code error;
    std::filesystem::create_directories(root_, error);
    if (error) {
        return Outcome<bool>::failure(storageError("Could not create BeatFlow's account data folder."));
    }
    auto temporary = path;
    temporary += ".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (!output) {
            return Outcome<bool>::failure(storageError("Could not write encrypted account data."));
        }
    }
    chmod(temporary.c_str(), S_IRUSR | S_IWUSR);
    std::filesystem::rename(temporary, path, error);
    if (error) {
        std::filesystem::remove(path, error);
        error.clear();
        std::filesystem::rename(temporary, path, error);
    }
    if (error) {
        return Outcome<bool>::failure(storageError("Could not publish encrypted account data."));
    }
    return Outcome<bool>::success(true);
}

} // namespace beatflow::quest
