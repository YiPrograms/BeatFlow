#include "beatflow/services/OAuthClient.hpp"

#include <nlohmann/json.hpp>

#include <cctype>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <sstream>

namespace beatflow {
namespace {

constexpr auto kDeviceCodeUrl = "https://oauth2.googleapis.com/device/code";
constexpr auto kTokenUrl = "https://oauth2.googleapis.com/token";
constexpr auto kScope = "https://www.googleapis.com/auth/youtube";
constexpr auto kDeviceGrant = "urn:ietf:params:oauth:grant-type:device_code";

std::int64_t nowEpochSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

std::string urlEncode(const std::string& value) {
    std::ostringstream encoded;
    encoded << std::uppercase << std::hex;
    for (const unsigned char character : value) {
        if (std::isalnum(character) != 0 || character == '-' || character == '_' || character == '.' ||
            character == '~') {
            encoded << static_cast<char>(character);
        } else {
            encoded << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(character);
        }
    }
    return encoded.str();
}

std::string form(const std::vector<std::pair<std::string, std::string>>& values) {
    std::ostringstream result;
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index != 0) {
            result << '&';
        }
        result << urlEncode(values[index].first) << '=' << urlEncode(values[index].second);
    }
    return result.str();
}

ServiceError responseError(const HttpResponse& response, const std::string& fallback) {
    std::string message = fallback;
    try {
        const auto json = nlohmann::json::parse(response.body);
        message = json.value("error_description", json.value("error", fallback));
    } catch (...) {
    }
    return {response.status == 429 ? ErrorCode::RateLimited : ErrorCode::Authentication, std::move(message),
            response.status == 429 || response.status >= 500, std::nullopt};
}

Outcome<nlohmann::json> parseJson(const HttpResponse& response) {
    try {
        return Outcome<nlohmann::json>::success(nlohmann::json::parse(response.body));
    } catch (const std::exception& exception) {
        return Outcome<nlohmann::json>::failure(
            {ErrorCode::InvalidResponse, std::string("OAuth returned invalid JSON: ") + exception.what(),
             false, std::nullopt});
    }
}

} // namespace

OAuthClient::OAuthClient(HttpClient& http, CredentialStore& credentials)
    : http_(http), credentials_(credentials) {}

Outcome<DeviceAuthorization> OAuthClient::begin(const CancellationToken& cancellation) {
    auto credentials = credentials_.loadClientCredentials();
    if (!credentials) {
        return Outcome<DeviceAuthorization>::failure(credentials.error());
    }
    if (credentials.value().clientId.empty() || credentials.value().clientSecret.empty()) {
        return Outcome<DeviceAuthorization>::failure(
            {ErrorCode::Configuration,
             "This BeatFlow build does not include its Google OAuth client credentials.", false,
             std::nullopt});
    }

    HttpRequest request;
    request.method = HttpRequest::Method::Post;
    request.url = kDeviceCodeUrl;
    request.headers = {{"Content-Type", "application/x-www-form-urlencoded"},
                       {"User-Agent", "Mozilla/5.0 Cobalt/Version BeatFlow/0.1"}};
    request.body = form({{"client_id", credentials.value().clientId}, {"scope", kScope}});
    auto response = http_.send(request, cancellation);
    if (!response) {
        return Outcome<DeviceAuthorization>::failure(response.error());
    }
    if (response.value().status < 200 || response.value().status >= 300) {
        return Outcome<DeviceAuthorization>::failure(
            responseError(response.value(), "Google rejected the device authorization request."));
    }

    auto json = parseJson(response.value());
    if (!json) {
        return Outcome<DeviceAuthorization>::failure(json.error());
    }
    try {
        DeviceAuthorization result;
        result.deviceCode = json.value().at("device_code").get<std::string>();
        result.userCode = json.value().at("user_code").get<std::string>();
        result.verificationUrl = json.value().value(
            "verification_url", json.value().value("verification_uri", "https://google.com/device"));
        result.expiresInSeconds = json.value().value("expires_in", 1800);
        result.pollingIntervalSeconds = std::max(5, json.value().value("interval", 5));
        return Outcome<DeviceAuthorization>::success(std::move(result));
    } catch (const std::exception& exception) {
        return Outcome<DeviceAuthorization>::failure(
            {ErrorCode::InvalidResponse,
             std::string("The device authorization response was incomplete: ") + exception.what(), false,
             std::nullopt});
    }
}

Outcome<AuthorizationPoll> OAuthClient::poll(const DeviceAuthorization& authorization,
                                             const CancellationToken& cancellation) {
    auto credentials = credentials_.loadClientCredentials();
    if (!credentials) {
        return Outcome<AuthorizationPoll>::failure(credentials.error());
    }

    HttpRequest request;
    request.method = HttpRequest::Method::Post;
    request.url = kTokenUrl;
    request.headers = {{"Content-Type", "application/x-www-form-urlencoded"},
                       {"User-Agent", "Mozilla/5.0 Cobalt/Version BeatFlow/0.1"}};
    request.body = form({{"client_id", credentials.value().clientId},
                         {"client_secret", credentials.value().clientSecret},
                         {"device_code", authorization.deviceCode},
                         {"grant_type", kDeviceGrant}});
    auto response = http_.send(request, cancellation);
    if (!response) {
        return Outcome<AuthorizationPoll>::failure(response.error());
    }

    auto json = parseJson(response.value());
    if (!json) {
        return Outcome<AuthorizationPoll>::failure(json.error());
    }
    if (response.value().status >= 200 && response.value().status < 300 &&
        json.value().contains("access_token")) {
        OAuthTokens tokens;
        tokens.accessToken = json.value().at("access_token").get<std::string>();
        tokens.refreshToken = json.value().value("refresh_token", "");
        tokens.tokenType = json.value().value("token_type", "Bearer");
        tokens.expiresAtEpochSeconds = nowEpochSeconds() + json.value().value("expires_in", 3600);
        if (tokens.refreshToken.empty()) {
            return Outcome<AuthorizationPoll>::failure(
                {ErrorCode::InvalidResponse, "Google did not return a refresh token.", false, std::nullopt});
        }
        auto saved = credentials_.saveTokens(tokens);
        if (!saved) {
            return Outcome<AuthorizationPoll>::failure(saved.error());
        }
        return Outcome<AuthorizationPoll>::success(
            {AuthorizationStatus::Complete, std::move(tokens), authorization.pollingIntervalSeconds});
    }

    const auto error = json.value().value("error", "unknown_error");
    if (error == "authorization_pending") {
        return Outcome<AuthorizationPoll>::success(
            {AuthorizationStatus::Pending, std::nullopt, authorization.pollingIntervalSeconds});
    }
    if (error == "slow_down") {
        return Outcome<AuthorizationPoll>::success(
            {AuthorizationStatus::SlowDown, std::nullopt, authorization.pollingIntervalSeconds + 5});
    }
    if (error == "expired_token") {
        return Outcome<AuthorizationPoll>::success(
            {AuthorizationStatus::Expired, std::nullopt, authorization.pollingIntervalSeconds});
    }
    if (error == "access_denied") {
        return Outcome<AuthorizationPoll>::success(
            {AuthorizationStatus::Denied, std::nullopt, authorization.pollingIntervalSeconds});
    }
    return Outcome<AuthorizationPoll>::failure(
        responseError(response.value(), "Google rejected the authorization request."));
}

Outcome<std::string> OAuthClient::accessToken(const CancellationToken& cancellation) {
    auto stored = credentials_.loadTokens();
    if (!stored) {
        return Outcome<std::string>::failure(stored.error());
    }
    if (!stored.value()) {
        return Outcome<std::string>::failure({ErrorCode::Authentication,
                                              "Connect YouTube Music before requesting recommendations.",
                                              false, std::nullopt});
    }
    auto tokens = *stored.value();
    if (tokens.expiresAtEpochSeconds - nowEpochSeconds() <= 60) {
        auto credentials = credentials_.loadClientCredentials();
        if (!credentials) {
            return Outcome<std::string>::failure(credentials.error());
        }
        auto refreshed = refresh(credentials.value(), tokens, cancellation);
        if (!refreshed) {
            return Outcome<std::string>::failure(refreshed.error());
        }
        tokens = std::move(refreshed).value();
    }
    return Outcome<std::string>::success(tokens.tokenType + " " + tokens.accessToken);
}

Outcome<std::string> OAuthClient::accountCacheNamespace() {
    auto stored = credentials_.loadTokens();
    if (!stored) {
        return Outcome<std::string>::failure(stored.error());
    }
    if (!stored.value() || stored.value()->refreshToken.empty()) {
        return Outcome<std::string>::failure({ErrorCode::Authentication,
                                              "Connect YouTube Music before requesting recommendations.",
                                              false, std::nullopt});
    }
    std::uint64_t hash = 1469598103934665603ULL;
    for (const unsigned char character : stored.value()->refreshToken) {
        hash ^= character;
        hash *= 1099511628211ULL;
    }
    std::ostringstream value;
    value << std::hex << hash;
    return Outcome<std::string>::success(value.str());
}

Outcome<bool> OAuthClient::disconnect() {
    return credentials_.clearTokens();
}

Outcome<OAuthTokens> OAuthClient::refresh(const OAuthClientCredentials& credentials,
                                          const OAuthTokens& tokens, const CancellationToken& cancellation) {
    HttpRequest request;
    request.method = HttpRequest::Method::Post;
    request.url = kTokenUrl;
    request.headers = {{"Content-Type", "application/x-www-form-urlencoded"},
                       {"User-Agent", "Mozilla/5.0 Cobalt/Version BeatFlow/0.1"}};
    request.body = form({{"client_id", credentials.clientId},
                         {"client_secret", credentials.clientSecret},
                         {"refresh_token", tokens.refreshToken},
                         {"grant_type", "refresh_token"}});
    auto response = http_.send(request, cancellation);
    if (!response) {
        return Outcome<OAuthTokens>::failure(response.error());
    }
    if (response.value().status < 200 || response.value().status >= 300) {
        return Outcome<OAuthTokens>::failure(
            responseError(response.value(), "Google could not refresh the YouTube Music session."));
    }
    auto json = parseJson(response.value());
    if (!json) {
        return Outcome<OAuthTokens>::failure(json.error());
    }
    try {
        auto refreshed = tokens;
        refreshed.accessToken = json.value().at("access_token").get<std::string>();
        refreshed.tokenType = json.value().value("token_type", tokens.tokenType);
        refreshed.expiresAtEpochSeconds = nowEpochSeconds() + json.value().value("expires_in", 3600);
        auto saved = credentials_.saveTokens(refreshed);
        if (!saved) {
            return Outcome<OAuthTokens>::failure(saved.error());
        }
        return Outcome<OAuthTokens>::success(std::move(refreshed));
    } catch (const std::exception& exception) {
        return Outcome<OAuthTokens>::failure(
            {ErrorCode::InvalidResponse,
             std::string("The token refresh response was incomplete: ") + exception.what(), false,
             std::nullopt});
    }
}

} // namespace beatflow
