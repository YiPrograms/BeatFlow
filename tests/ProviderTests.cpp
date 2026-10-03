#include "Test.hpp"

#include "beatflow/services/BeatSaverCatalog.hpp"
#include "beatflow/services/OAuthClient.hpp"
#include "beatflow/services/YouTubeMusicProvider.hpp"

#include <fstream>
#include <limits>
#include <queue>
#include <sstream>
#include <string_view>
#include <unordered_map>

using namespace beatflow;

namespace {

std::string fixture(const std::string& name) {
    std::ifstream input(std::string(BEATFLOW_FIXTURE_DIR) + "/" + name);
    std::ostringstream contents;
    contents << input.rdbuf();
    return contents.str();
}

class FakeHttp final : public HttpClient {
  public:
    std::queue<Outcome<HttpResponse>> responses;
    std::vector<HttpRequest> requests;

    Outcome<HttpResponse> send(const HttpRequest& request, const CancellationToken&) override {
        requests.push_back(request);
        if (responses.empty()) {
            return Outcome<HttpResponse>::failure(
                {ErrorCode::Network, "No fake response configured.", false, std::nullopt});
        }
        auto result = std::move(responses.front());
        responses.pop();
        return result;
    }
};

class MemoryCredentials final : public CredentialStore {
  public:
    OAuthClientCredentials client{"client-id", "client-secret"};
    std::optional<OAuthTokens> tokens;

    Outcome<OAuthClientCredentials> loadClientCredentials() override {
        return Outcome<OAuthClientCredentials>::success(client);
    }
    Outcome<std::optional<OAuthTokens>> loadTokens() override {
        return Outcome<std::optional<OAuthTokens>>::success(tokens);
    }
    Outcome<bool> saveTokens(const OAuthTokens& value) override {
        tokens = value;
        return Outcome<bool>::success(true);
    }
    Outcome<bool> clearTokens() override {
        tokens.reset();
        return Outcome<bool>::success(true);
    }
};

class MemoryCache final : public CacheStore {
  public:
    Outcome<std::string> read(const std::string& key) override {
        const auto item = values.find(key);
        if (item == values.end()) {
            return Outcome<std::string>::failure(
                {ErrorCode::NotFound, "Missing cache entry.", false, std::nullopt});
        }
        return Outcome<std::string>::success(item->second);
    }

    Outcome<bool> write(const std::string& key, const std::string& value) override {
        values[key] = value;
        return Outcome<bool>::success(true);
    }

    Outcome<bool> remove(const std::string& key) override {
        values.erase(key);
        return Outcome<bool>::success(true);
    }

    Outcome<bool> clear() override {
        values.clear();
        return Outcome<bool>::success(true);
    }

    std::unordered_map<std::string, std::string> values;
};

std::optional<std::string> headerValue(const HttpRequest& request, std::string_view name) {
    for (const auto& [headerName, value] : request.headers) {
        if (headerName == name) {
            return value;
        }
    }
    return std::nullopt;
}

} // namespace

BF_TEST("YouTube Home parser ignores non-track cards and keeps track metadata") {
    FakeHttp http;
    YouTubeMusicProvider provider(http);
    const auto result = provider.parseTracks(fixture("youtube_home.json"), "Home");
    BF_REQUIRE(result.ok());
    BF_REQUIRE(result.value().size() == 1);
    BF_REQUIRE(result.value().front().providerId == "video-idol");
    BF_REQUIRE(result.value().front().title == "Idol");
    BF_REQUIRE(result.value().front().durationSeconds == 214);
    BF_REQUIRE(!result.value().front().artists.empty());
    BF_REQUIRE(result.value().front().artists.front() == "YOASOBI");
}

BF_TEST("YouTube radio parser reads playlist-panel tracks") {
    FakeHttp http;
    YouTubeMusicProvider provider(http);
    const auto result = provider.parseTracks(fixture("youtube_radio.json"), "Radio");
    BF_REQUIRE(result.ok());
    BF_REQUIRE(result.value().size() == 2);
    BF_REQUIRE(result.value()[1].providerId == "next-track");
    BF_REQUIRE(result.value()[1].durationSeconds == 252);
}

BF_TEST("YouTube search parser treats shelf headings as containers") {
    FakeHttp http;
    YouTubeMusicProvider provider(http);
    const auto result = provider.parseTracks(fixture("youtube_search.json"), "Search");

    BF_REQUIRE(result.ok());
    BF_REQUIRE(result.value().size() == 1);
    BF_REQUIRE(result.value().front().title == "アイドル");
    BF_REQUIRE(result.value().front().artists.front() == "YOASOBI");
    BF_REQUIRE(result.value().front().artworkUrl == "https://example.test/idol.jpg");
}

BF_TEST("YouTube search and Up Next work without a connected account") {
    FakeHttp http;
    YouTubeMusicProvider provider(http);
    http.responses.push(Outcome<HttpResponse>::success({200, {}, fixture("youtube_home.json")}));
    http.responses.push(Outcome<HttpResponse>::success({200, {}, fixture("youtube_radio.json")}));
    CancellationSource cancellation;

    const auto search = provider.search("Idol YOASOBI", cancellation.token());
    const auto upNext = provider.radio("video-idol", cancellation.token());

    BF_REQUIRE(search.ok());
    BF_REQUIRE(search.value().size() == 1);
    BF_REQUIRE(upNext.ok());
    BF_REQUIRE(upNext.value().size() == 2);
    BF_REQUIRE(http.requests.size() == 2);
    BF_REQUIRE(http.requests[0].url.find("/search?") != std::string::npos);
    BF_REQUIRE(http.requests[1].url.find("/next?") != std::string::npos);
    BF_REQUIRE(!headerValue(http.requests[0], "Authorization").has_value());
    BF_REQUIRE(!headerValue(http.requests[1], "Authorization").has_value());
    BF_REQUIRE(headerValue(http.requests[1], "X-Youtube-Client-Name") == "67");
    BF_REQUIRE(http.requests[1].body.find("\"videoId\":\"video-idol\"") != std::string::npos);
    BF_REQUIRE(http.requests[1].body.find("\"clientName\":\"WEB_REMIX\"") != std::string::npos);
}

BF_TEST("YouTube requests retain a marked stale result while offline") {
    FakeHttp http;
    MemoryCache cache;
    YouTubeMusicProvider provider(http, &cache);
    http.responses.push(Outcome<HttpResponse>::success({200, {}, fixture("youtube_home.json")}));
    http.responses.push(Outcome<HttpResponse>::failure({ErrorCode::Network, "offline", true, std::nullopt}));
    CancellationSource cancellation;

    const auto fresh = provider.search("Idol YOASOBI", cancellation.token());
    const auto stale = provider.search("Idol YOASOBI", cancellation.token());

    BF_REQUIRE(fresh.ok());
    BF_REQUIRE(!fresh.value().front().stale);
    BF_REQUIRE(stale.ok());
    BF_REQUIRE(stale.value().front().stale);
    BF_REQUIRE(cache.values.size() == 1);
}

BF_TEST("YouTube requests discard corrupt cache envelopes") {
    FakeHttp http;
    MemoryCache cache;
    YouTubeMusicProvider provider(http, &cache);
    http.responses.push(Outcome<HttpResponse>::success({200, {}, fixture("youtube_home.json")}));
    http.responses.push(Outcome<HttpResponse>::failure({ErrorCode::Network, "offline", true, std::nullopt}));
    CancellationSource cancellation;

    BF_REQUIRE(provider.search("Idol YOASOBI", cancellation.token()).ok());
    BF_REQUIRE(cache.values.size() == 1);
    cache.values.begin()->second = "{broken";
    const auto result = provider.search("Idol YOASOBI", cancellation.token());

    BF_REQUIRE(!result.ok());
    BF_REQUIRE(result.error().code == ErrorCode::Network);
    BF_REQUIRE(cache.values.empty());
}

BF_TEST("personalized YouTube Music Home still requires a connected account") {
    FakeHttp http;
    YouTubeMusicProvider provider(http);
    CancellationSource cancellation;

    const auto result = provider.home(cancellation.token());

    BF_REQUIRE(!result.ok());
    BF_REQUIRE(result.error().code == ErrorCode::Authentication);
    BF_REQUIRE(http.requests.empty());
}

BF_TEST("connected YouTube Music Home sends authentication for personalized shelves") {
    FakeHttp http;
    MemoryCredentials credentials;
    credentials.tokens = OAuthTokens{"access", "refresh", "Bearer", std::numeric_limits<std::int64_t>::max()};
    OAuthClient oauth(http, credentials);
    YouTubeMusicProvider provider(http, oauth);
    http.responses.push(Outcome<HttpResponse>::success({200, {}, fixture("youtube_home.json")}));
    http.responses.push(Outcome<HttpResponse>::success({200, {}, fixture("youtube_radio.json")}));
    CancellationSource cancellation;

    const auto result = provider.home(cancellation.token());

    BF_REQUIRE(result.ok());
    BF_REQUIRE(result.value().size() == 3);
    BF_REQUIRE(http.requests.size() == 2);
    BF_REQUIRE(headerValue(http.requests[0], "Authorization") == "Bearer access");
    BF_REQUIRE(headerValue(http.requests[1], "Authorization") == "Bearer access");
}

BF_TEST("personalized caches cannot cross account token namespaces") {
    FakeHttp http;
    MemoryCache cache;
    MemoryCredentials credentials;
    credentials.tokens =
        OAuthTokens{"access-a", "refresh-a", "Bearer", std::numeric_limits<std::int64_t>::max()};
    OAuthClient oauth(http, credentials);
    YouTubeMusicProvider provider(http, oauth, nullptr, &cache);
    http.responses.push(Outcome<HttpResponse>::success({200, {}, fixture("youtube_search.json")}));
    CancellationSource cancellation;

    const auto accountA = provider.home(cancellation.token());
    BF_REQUIRE(accountA.ok());
    BF_REQUIRE(cache.values.size() == 1);

    credentials.tokens =
        OAuthTokens{"access-b", "refresh-b", "Bearer", std::numeric_limits<std::int64_t>::max()};
    http.responses.push(Outcome<HttpResponse>::failure({ErrorCode::Network, "offline", true, std::nullopt}));
    const auto accountB = provider.home(cancellation.token());

    BF_REQUIRE(!accountB.ok());
    BF_REQUIRE(accountB.error().code == ErrorCode::Network);
}

BF_TEST("YouTube parser reports malformed JSON without throwing") {
    FakeHttp http;
    YouTubeMusicProvider provider(http);
    const auto result = provider.parseTracks("{bad", "Home");
    BF_REQUIRE(!result.ok());
    BF_REQUIRE(result.error().code == ErrorCode::InvalidResponse);
}

BF_TEST("BeatSaver parser selects published version and exposes requirements") {
    FakeHttp http;
    BeatSaverCatalog catalog(http);
    const auto result = catalog.parseSearchResponse(fixture("beatsaver_search.json"));
    BF_REQUIRE(result.ok());
    BF_REQUIRE(result.value().size() == 1);
    const auto& map = result.value().front();
    BF_REQUIRE(map.key == "316ed");
    BF_REQUIRE(map.curated);
    BF_REQUIRE(map.difficulties.size() == 2);
    BF_REQUIRE(map.difficulties[1].requirements.size() == 1);
}

BF_TEST("BeatSaver cache envelopes fall back with stale map metadata") {
    FakeHttp http;
    MemoryCache cache;
    BeatSaverCatalog catalog(http, &cache);
    Track track{"video", "Idol", {"YOASOBI"}, 214, "", "Radio", 1.0};
    http.responses.push(Outcome<HttpResponse>::success({200, {}, fixture("beatsaver_search.json")}));
    http.responses.push(Outcome<HttpResponse>::failure({ErrorCode::Network, "offline", true, std::nullopt}));
    CancellationSource cancellation;

    const auto fresh = catalog.search(track, cancellation.token());
    const auto stale = catalog.search(track, cancellation.token());

    BF_REQUIRE(fresh.ok());
    BF_REQUIRE(!fresh.value().front().stale);
    BF_REQUIRE(stale.ok());
    BF_REQUIRE(stale.value().front().stale);
}

BF_TEST("device authorization persists completed OAuth tokens") {
    FakeHttp http;
    MemoryCredentials credentials;
    OAuthClient oauth(http, credentials);
    http.responses.push(Outcome<HttpResponse>::success(
        {200,
         {},
         R"({"device_code":"device","user_code":"ABCD-EFGH","verification_url":"https://google.com/device","expires_in":1800,"interval":5})"}));
    http.responses.push(Outcome<HttpResponse>::success(
        {200,
         {},
         R"({"access_token":"access","refresh_token":"refresh","expires_in":3600,"token_type":"Bearer"})"}));

    CancellationSource cancellation;
    const auto authorization = oauth.begin(cancellation.token());
    BF_REQUIRE(authorization.ok());
    BF_REQUIRE(authorization.value().userCode == "ABCD-EFGH");
    const auto poll = oauth.poll(authorization.value(), cancellation.token());
    BF_REQUIRE(poll.ok());
    BF_REQUIRE(poll.value().status == AuthorizationStatus::Complete);
    BF_REQUIRE(credentials.tokens.has_value());
    BF_REQUIRE(credentials.tokens->refreshToken == "refresh");
}

BF_TEST("OAuth pending response remains a non-error state") {
    FakeHttp http;
    MemoryCredentials credentials;
    OAuthClient oauth(http, credentials);
    http.responses.push(Outcome<HttpResponse>::success({400, {}, R"({"error":"authorization_pending"})"}));
    CancellationSource cancellation;
    const DeviceAuthorization authorization{"device", "code", "url", 1800, 5};
    const auto poll = oauth.poll(authorization, cancellation.token());
    BF_REQUIRE(poll.ok());
    BF_REQUIRE(poll.value().status == AuthorizationStatus::Pending);
}

BF_TEST("expired OAuth access tokens refresh and persist before use") {
    FakeHttp http;
    MemoryCredentials credentials;
    credentials.tokens = OAuthTokens{"expired", "refresh", "Bearer", 0};
    OAuthClient oauth(http, credentials);
    http.responses.push(Outcome<HttpResponse>::success(
        {200, {}, R"({"access_token":"renewed","expires_in":3600,"token_type":"Bearer"})"}));
    CancellationSource cancellation;

    const auto authorization = oauth.accessToken(cancellation.token());

    BF_REQUIRE(authorization.ok());
    BF_REQUIRE(authorization.value() == "Bearer renewed");
    BF_REQUIRE(credentials.tokens.has_value());
    BF_REQUIRE(credentials.tokens->accessToken == "renewed");
    BF_REQUIRE(credentials.tokens->refreshToken == "refresh");
    BF_REQUIRE(http.requests.size() == 1);
    BF_REQUIRE(http.requests.front().url.find("oauth2.googleapis.com/token") != std::string::npos);
}
