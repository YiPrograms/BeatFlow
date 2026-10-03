#pragma once

#include "beatflow/core/Interfaces.hpp"

#include <chrono>
#include <optional>
#include <string>

namespace beatflow {

struct OAuthClientCredentials {
    std::string clientId;
    std::string clientSecret;
};

struct OAuthTokens {
    std::string accessToken;
    std::string refreshToken;
    std::string tokenType{"Bearer"};
    std::int64_t expiresAtEpochSeconds{0};
};

struct DeviceAuthorization {
    std::string deviceCode;
    std::string userCode;
    std::string verificationUrl;
    int expiresInSeconds{0};
    int pollingIntervalSeconds{5};
};

enum class AuthorizationStatus { Pending, SlowDown, Complete, Expired, Denied };

struct AuthorizationPoll {
    AuthorizationStatus status{AuthorizationStatus::Pending};
    std::optional<OAuthTokens> tokens;
    int nextPollSeconds{5};
};

class CredentialStore {
  public:
    virtual ~CredentialStore() = default;
    virtual Outcome<OAuthClientCredentials> loadClientCredentials() = 0;
    virtual Outcome<std::optional<OAuthTokens>> loadTokens() = 0;
    virtual Outcome<bool> saveTokens(const OAuthTokens& tokens) = 0;
    virtual Outcome<bool> clearTokens() = 0;
};

class OAuthClient {
  public:
    OAuthClient(HttpClient& http, CredentialStore& credentials);

    Outcome<DeviceAuthorization> begin(const CancellationToken& cancellation);
    Outcome<AuthorizationPoll> poll(const DeviceAuthorization& authorization,
                                    const CancellationToken& cancellation);
    Outcome<std::string> accessToken(const CancellationToken& cancellation);
    Outcome<std::string> accountCacheNamespace();
    Outcome<bool> disconnect();

  private:
    Outcome<OAuthTokens> refresh(const OAuthClientCredentials& credentials, const OAuthTokens& tokens,
                                 const CancellationToken& cancellation);

    HttpClient& http_;
    CredentialStore& credentials_;
};

} // namespace beatflow
