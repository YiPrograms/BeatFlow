#include "beatflow/services/YouTubeMusicProvider.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <optional>
#include <set>
#include <sstream>
#include <unordered_set>

namespace beatflow {
namespace {

using Json = nlohmann::json;

constexpr auto kApiBase = "https://music.youtube.com/youtubei/v1/";
constexpr auto kYouTubeDataApiBase = "https://www.googleapis.com/youtube/v3/";
// This is YouTube Music's public web client key, matching the WEB_REMIX client used by ytmusicapi.
constexpr auto kWebClientKey = "AIzaSyC9XL3ZjWddXya6X74dJoCTL-WEYFDNX30";
constexpr std::size_t kPersonalizedRadioSeeds = 6;

std::string webClientVersion() {
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::tm utc{};
#if defined(_WIN32)
    gmtime_s(&utc, &time);
#else
    gmtime_r(&time, &utc);
#endif
    std::ostringstream version;
    version << "1." << std::put_time(&utc, "%Y%m%d") << ".01.00";
    return version.str();
}

std::optional<std::string> textValue(const Json& value) {
    if (value.is_string()) {
        return value.get<std::string>();
    }
    if (!value.is_object()) {
        return std::nullopt;
    }
    if (value.contains("simpleText") && value["simpleText"].is_string()) {
        return value["simpleText"].get<std::string>();
    }
    if (value.contains("runs") && value["runs"].is_array()) {
        std::string text;
        for (const auto& run : value["runs"]) {
            if (run.is_object() && run.contains("text") && run["text"].is_string()) {
                text += run["text"].get<std::string>();
            }
        }
        if (!text.empty()) {
            return text;
        }
    }
    return std::nullopt;
}

std::optional<std::string> findStringKey(const Json& value, const std::string& key) {
    if (value.is_object()) {
        const auto iterator = value.find(key);
        if (iterator != value.end() && iterator->is_string()) {
            return iterator->get<std::string>();
        }
        for (const auto& [_, child] : value.items()) {
            if (auto found = findStringKey(child, key)) {
                return found;
            }
        }
    } else if (value.is_array()) {
        for (const auto& child : value) {
            if (auto found = findStringKey(child, key)) {
                return found;
            }
        }
    }
    return std::nullopt;
}

std::optional<std::string> findArtworkUrl(const Json& value) {
    if (value.is_object()) {
        const auto thumbnails = value.find("thumbnails");
        if (thumbnails != value.end() && thumbnails->is_array()) {
            std::optional<std::string> result;
            for (const auto& thumbnail : *thumbnails) {
                if (thumbnail.is_object() && thumbnail.contains("url") && thumbnail["url"].is_string()) {
                    result = thumbnail["url"].get<std::string>();
                }
            }
            if (result) {
                return result;
            }
        }
        for (const auto& [_, child] : value.items()) {
            if (auto result = findArtworkUrl(child)) {
                return result;
            }
        }
    } else if (value.is_array()) {
        for (const auto& child : value) {
            if (auto result = findArtworkUrl(child)) {
                return result;
            }
        }
    }
    return std::nullopt;
}

std::optional<int> parseDuration(const std::string& value) {
    std::vector<int> fields;
    std::istringstream stream(value);
    for (std::string field; std::getline(stream, field, ':');) {
        if (field.empty() ||
            !std::all_of(field.begin(), field.end(), [](unsigned char c) { return std::isdigit(c) != 0; })) {
            return std::nullopt;
        }
        fields.push_back(std::stoi(field));
    }
    if (fields.size() == 2) {
        return fields[0] * 60 + fields[1];
    }
    if (fields.size() == 3) {
        return fields[0] * 3600 + fields[1] * 60 + fields[2];
    }
    return std::nullopt;
}

std::optional<int> findDuration(const Json& value) {
    if (value.is_string()) {
        return parseDuration(value.get<std::string>());
    }
    if (value.is_object()) {
        if (auto renderedText = textValue(value)) {
            if (auto duration = parseDuration(*renderedText)) {
                return duration;
            }
        }
        for (const auto* key : {"duration_seconds", "durationSeconds"}) {
            const auto iterator = value.find(key);
            if (iterator != value.end() && iterator->is_number_integer()) {
                return iterator->get<int>();
            }
        }
        for (const auto* key : {"duration", "length", "lengthText"}) {
            const auto iterator = value.find(key);
            if (iterator != value.end()) {
                if (auto text = textValue(*iterator)) {
                    if (auto duration = parseDuration(*text)) {
                        return duration;
                    }
                }
            }
        }
        for (const auto& [_, child] : value.items()) {
            if (auto duration = findDuration(child)) {
                return duration;
            }
        }
    } else if (value.is_array()) {
        for (const auto& child : value) {
            if (auto duration = findDuration(child)) {
                return duration;
            }
        }
    }
    return std::nullopt;
}

void collectArtists(const Json& value, std::vector<std::string>& artists) {
    if (value.is_object()) {
        const auto artistsNode = value.find("artists");
        if (artistsNode != value.end() && artistsNode->is_array()) {
            for (const auto& artist : *artistsNode) {
                if (artist.is_string()) {
                    artists.push_back(artist.get<std::string>());
                } else if (artist.is_object() && artist.contains("name") && artist["name"].is_string()) {
                    artists.push_back(artist["name"].get<std::string>());
                }
            }
        }

        if (value.contains("text") && value["text"].is_string() && value.contains("navigationEndpoint")) {
            const auto dump = value["navigationEndpoint"].dump();
            if (dump.find("MUSIC_PAGE_TYPE_ARTIST") != std::string::npos) {
                artists.push_back(value["text"].get<std::string>());
            }
        }
        for (const auto& [key, child] : value.items()) {
            if (key != "artists") {
                collectArtists(child, artists);
            }
        }
    } else if (value.is_array()) {
        for (const auto& child : value) {
            collectArtists(child, artists);
        }
    }
}

std::optional<std::string> findTitle(const Json& renderer) {
    const auto direct = renderer.find("title");
    if (direct != renderer.end()) {
        if (auto result = textValue(*direct)) {
            return result;
        }
    }
    const auto columns = renderer.find("flexColumns");
    if (columns != renderer.end() && columns->is_array() && !columns->empty()) {
        if (auto result = findStringKey(columns->front(), "text")) {
            return result;
        }
    }
    return std::nullopt;
}

std::optional<Track> extractTrack(const Json& renderer, const std::string& source) {
    auto videoId = findStringKey(renderer, "videoId");
    auto title = findTitle(renderer);
    if (!videoId || !title || videoId->empty() || title->empty()) {
        return std::nullopt;
    }

    Track result;
    result.providerId = *videoId;
    result.title = *title;
    result.durationSeconds = findDuration(renderer);
    result.sourceShelf = source;
    collectArtists(renderer, result.artists);
    std::unordered_set<std::string> seenArtists;
    result.artists.erase(std::remove_if(result.artists.begin(), result.artists.end(),
                                        [&seenArtists](const auto& artist) {
                                            return artist.empty() || !seenArtists.insert(artist).second;
                                        }),
                         result.artists.end());

    if (auto artwork = findArtworkUrl(renderer)) {
        result.artworkUrl = *artwork;
    }
    return result;
}

void walkRenderers(const Json& value, const std::string& source, std::vector<Track>& tracks,
                   std::unordered_set<std::string>& seen) {
    static const std::set<std::string> rendererNames{
        "musicResponsiveListItemRenderer",
        "musicTwoRowItemRenderer",
        "playlistPanelVideoRenderer",
    };

    if (value.is_object()) {
        if (value.contains("videoId") && value.contains("title")) {
            if (auto track = extractTrack(value, source); track && seen.insert(track->providerId).second) {
                tracks.push_back(std::move(*track));
            }
        }
        for (const auto& [key, child] : value.items()) {
            if (rendererNames.contains(key)) {
                if (auto track = extractTrack(child, source);
                    track && seen.insert(track->providerId).second) {
                    tracks.push_back(std::move(*track));
                }
            }
            walkRenderers(child, source, tracks, seen);
        }
    } else if (value.is_array()) {
        for (const auto& child : value) {
            walkRenderers(child, source, tracks, seen);
        }
    }
}

ServiceError httpError(const HttpResponse& response) {
    ErrorCode code = ErrorCode::Network;
    bool retryable = response.status == 429 || response.status >= 500;
    if (response.status == 401 || response.status == 403) {
        code = ErrorCode::Authentication;
    } else if (response.status == 429) {
        code = ErrorCode::RateLimited;
    } else if (response.status >= 400 && response.status < 500) {
        code = ErrorCode::InvalidResponse;
    }
    std::string message = "YouTube Music returned HTTP " + std::to_string(response.status) + ".";
    try {
        const auto json = Json::parse(response.body);
        if (json.contains("error") && json["error"].is_object()) {
            message = json["error"].value("message", message);
        }
    } catch (...) {
    }
    return {code, std::move(message), retryable, std::nullopt};
}

std::string cacheKey(const std::string& endpoint, const std::string& payload,
                     const std::string& cacheNamespace) {
    std::uint64_t hash = 1469598103934665603ULL;
    const auto input = endpoint + "\n" + payload;
    for (const unsigned char character : input) {
        hash ^= character;
        hash *= 1099511628211ULL;
    }
    std::ostringstream key;
    key << cacheNamespace << '_' << std::hex << hash;
    return key.str();
}

std::int64_t nowEpochSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

std::string urlEncode(std::string_view value) {
    constexpr char digits[] = "0123456789ABCDEF";
    std::string result;
    result.reserve(value.size());
    for (const unsigned char character : value) {
        if (std::isalnum(character) != 0 || character == '-' || character == '_' || character == '.' ||
            character == '~') {
            result.push_back(static_cast<char>(character));
        } else {
            result.push_back('%');
            result.push_back(digits[character >> 4U]);
            result.push_back(digits[character & 0x0FU]);
        }
    }
    return result;
}

std::string removeTopicSuffix(std::string value) {
    constexpr std::string_view suffix = " - Topic";
    if (value.ends_with(suffix)) {
        value.resize(value.size() - suffix.size());
    }
    return value;
}

} // namespace

YouTubeMusicProvider::YouTubeMusicProvider(HttpClient& http, CacheStore* anonymousCache, std::string language,
                                           std::string location)
    : http_(http), oauth_(nullptr), anonymousCache_(anonymousCache), accountCache_(nullptr),
      language_(std::move(language)), location_(std::move(location)) {}

YouTubeMusicProvider::YouTubeMusicProvider(HttpClient& http, OAuthClient& oauth, CacheStore* anonymousCache,
                                           CacheStore* accountCache, std::string language,
                                           std::string location)
    : http_(http), oauth_(&oauth), anonymousCache_(anonymousCache), accountCache_(accountCache),
      language_(std::move(language)), location_(std::move(location)) {}

Outcome<std::vector<Track>> YouTubeMusicProvider::home(const CancellationToken& cancellation) {
    auto seeds = likedVideos(cancellation);
    if (!seeds) {
        return seeds;
    }

    std::vector<Track> recommendations = seeds.value();
    std::unordered_set<std::string> seen;
    for (const auto& seed : seeds.value()) {
        seen.insert(seed.providerId);
    }
    const auto limit = std::min(kPersonalizedRadioSeeds, seeds.value().size());
    for (std::size_t index = 0; index < limit && !cancellation.isCancellationRequested(); ++index) {
        auto related = radio(seeds.value()[index].providerId, cancellation);
        if (!related) {
            if (related.error().code == ErrorCode::Cancelled) {
                return related;
            }
            continue;
        }
        for (auto& track : related.value()) {
            if (!track.providerId.empty() && seen.insert(track.providerId).second) {
                track.sourceShelf = "Recommended from your likes";
                recommendations.push_back(std::move(track));
            }
        }
    }
    return Outcome<std::vector<Track>>::success(std::move(recommendations));
}

Outcome<std::vector<Track>> YouTubeMusicProvider::likedVideos(const CancellationToken& cancellation) {
    auto channel =
        accountGet(std::string(kYouTubeDataApiBase) + "channels?part=contentDetails&mine=true", cancellation);
    if (!channel) {
        return Outcome<std::vector<Track>>::failure(channel.error());
    }

    try {
        const auto channelJson = Json::parse(channel.value().body);
        const auto& items = channelJson.at("items");
        if (!items.is_array() || items.empty()) {
            return Outcome<std::vector<Track>>::failure(
                {ErrorCode::NotFound,
                 "This Google account does not have a YouTube channel with a liked-videos playlist.", false,
                 std::nullopt});
        }
        const auto playlistId =
            items.front().at("contentDetails").at("relatedPlaylists").at("likes").get<std::string>();
        if (playlistId.empty()) {
            return Outcome<std::vector<Track>>::failure(
                {ErrorCode::NotFound, "This account's liked-videos playlist is unavailable.", false,
                 std::nullopt});
        }

        const auto url =
            std::string(kYouTubeDataApiBase) +
            "playlistItems?part=snippet%2CcontentDetails&maxResults=50&playlistId=" + urlEncode(playlistId);
        auto playlist = accountGet(url, cancellation);
        if (!playlist) {
            return Outcome<std::vector<Track>>::failure(playlist.error());
        }

        const auto playlistJson = Json::parse(playlist.value().body);
        std::vector<Track> tracks;
        std::unordered_set<std::string> seen;
        for (const auto& item : playlistJson.value("items", Json::array())) {
            if (!item.is_object() || !item.contains("snippet")) {
                continue;
            }
            const auto& snippet = item["snippet"];
            auto videoId = item.value("contentDetails", Json::object()).value("videoId", "");
            if (videoId.empty()) {
                videoId = snippet.value("resourceId", Json::object()).value("videoId", "");
            }
            const auto title = snippet.value("title", "");
            if (videoId.empty() || title.empty() || title == "Deleted video" || title == "Private video" ||
                !seen.insert(videoId).second) {
                continue;
            }

            Track track;
            track.providerId = std::move(videoId);
            track.title = title;
            const auto artist = removeTopicSuffix(snippet.value("videoOwnerChannelTitle", ""));
            if (!artist.empty()) {
                track.artists.push_back(artist);
            }
            track.sourceShelf = "Your liked videos";
            track.providerRelevance = std::max(0.55, 1.0 - static_cast<double>(tracks.size()) / 100.0);
            track.stale = channel.value().stale || playlist.value().stale;
            if (snippet.contains("thumbnails") && snippet["thumbnails"].is_object()) {
                int bestWidth = -1;
                for (const auto& [_, thumbnail] : snippet["thumbnails"].items()) {
                    if (thumbnail.is_object() && thumbnail.contains("url") && thumbnail["url"].is_string()) {
                        const auto width = thumbnail.value("width", 0);
                        if (width >= bestWidth) {
                            bestWidth = width;
                            track.artworkUrl = thumbnail["url"].get<std::string>();
                        }
                    }
                }
            }
            tracks.push_back(std::move(track));
        }
        if (tracks.empty()) {
            return Outcome<std::vector<Track>>::failure(
                {ErrorCode::NotFound, "No usable videos were found in this account's liked-videos playlist.",
                 false, std::nullopt});
        }
        return Outcome<std::vector<Track>>::success(std::move(tracks));
    } catch (const std::exception& exception) {
        return Outcome<std::vector<Track>>::failure(
            {ErrorCode::InvalidResponse,
             std::string("YouTube returned incomplete account data: ") + exception.what(), false,
             std::nullopt});
    }
}

Outcome<std::vector<Track>> YouTubeMusicProvider::search(const std::string& query,
                                                         const CancellationToken& cancellation) {
    Json payload{{"query", query}, {"params", "EgWKAQIIAWoMEA4QChADEAQQCRAF"}};
    return request("search", payload.dump(), "YouTube Music search", cancellation);
}

Outcome<std::vector<Track>> YouTubeMusicProvider::radio(const std::string& trackId,
                                                        const CancellationToken& cancellation) {
    Json payload{{"enablePersistentPlaylistPanel", true},
                 {"isAudioOnly", true},
                 {"tunerSettingValue", "AUTOMIX_SETTING_NORMAL"},
                 {"videoId", trackId},
                 {"playlistId", "RDAMVM" + trackId},
                 {"params", "wAEB"}};
    return request("next", payload.dump(), "Song radio", cancellation);
}

Outcome<std::vector<Track>> YouTubeMusicProvider::parseTracks(const std::string& response,
                                                              const std::string& source) const {
    try {
        const auto json = Json::parse(response);
        std::vector<Track> tracks;
        std::unordered_set<std::string> seen;
        walkRenderers(json, source, tracks, seen);
        for (std::size_t index = 0; index < tracks.size(); ++index) {
            const auto count = static_cast<double>(std::max<std::size_t>(tracks.size(), 1));
            tracks[index].providerRelevance = std::max(0.5, 1.0 - static_cast<double>(index) / count * 0.5);
        }
        return Outcome<std::vector<Track>>::success(std::move(tracks));
    } catch (const std::exception& exception) {
        return Outcome<std::vector<Track>>::failure(
            {ErrorCode::InvalidResponse,
             std::string("YouTube Music returned invalid JSON: ") + exception.what(), false, std::nullopt});
    }
}

Outcome<YouTubeMusicProvider::RawResponse>
YouTubeMusicProvider::rawRequest(const std::string& endpoint, const std::string& payload,
                                 const CancellationToken& cancellation) {
    if (cancellation.isCancellationRequested()) {
        return Outcome<RawResponse>::failure(
            {ErrorCode::Cancelled, "The YouTube Music request was cancelled.", false, std::nullopt});
    }
    Json body;
    try {
        body = Json::parse(payload);
    } catch (const std::exception& exception) {
        return Outcome<RawResponse>::failure(
            {ErrorCode::Internal, std::string("Invalid internal YouTube request: ") + exception.what(), false,
             std::nullopt});
    }
    const auto clientVersion = webClientVersion();
    body["context"] = Json::parse(contextPayload(clientVersion))["context"];

    HttpRequest request;
    request.method = HttpRequest::Method::Post;
    request.url = std::string(kApiBase) + endpoint + "?alt=json&key=" + kWebClientKey;
    request.headers = {{"Content-Type", "application/json"},
                       {"Origin", "https://music.youtube.com"},
                       {"User-Agent", "Mozilla/5.0 BeatFlow/0.1"},
                       {"X-Youtube-Client-Name", "67"},
                       {"X-Youtube-Client-Version", clientVersion},
                       {"X-Goog-Request-Time", std::to_string(std::time(nullptr))}};
    auto* cache = anonymousCache_;
    std::optional<ServiceError> failure;
    const auto key = cacheKey(endpoint, payload, "anonymous");
    request.body = body.dump();
    if (!failure) {
        auto response = http_.send(request, cancellation);
        if (!response) {
            failure = response.error();
        } else if (response.value().status < 200 || response.value().status >= 300) {
            failure = httpError(response.value());
        } else {
            if (cache != nullptr) {
                Json envelope{
                    {"schema", 1}, {"stored_at", nowEpochSeconds()}, {"body", response.value().body}};
                static_cast<void>(cache->write(key, envelope.dump()));
            }
            return Outcome<RawResponse>::success({std::move(response.value().body), false});
        }
    }

    if (!cancellation.isCancellationRequested() && cache != nullptr) {
        auto cached = cache->read(key);
        if (cached) {
            try {
                const auto envelope = Json::parse(cached.value());
                if (envelope.value("schema", 0) == 1 && envelope.contains("body") &&
                    envelope["body"].is_string()) {
                    return Outcome<RawResponse>::success({envelope["body"].get<std::string>(), true});
                }
            } catch (...) {
            }
            static_cast<void>(cache->remove(key));
        }
    }
    return Outcome<RawResponse>::failure(*failure);
}

Outcome<YouTubeMusicProvider::RawResponse>
YouTubeMusicProvider::accountGet(const std::string& url, const CancellationToken& cancellation) {
    if (cancellation.isCancellationRequested()) {
        return Outcome<RawResponse>::failure(
            {ErrorCode::Cancelled, "The YouTube account request was cancelled.", false, std::nullopt});
    }
    if (oauth_ == nullptr) {
        return Outcome<RawResponse>::failure({ErrorCode::Authentication,
                                              "Connect YouTube to request personalized recommendations.",
                                              false, std::nullopt});
    }

    auto accountNamespace = oauth_->accountCacheNamespace();
    if (!accountNamespace) {
        return Outcome<RawResponse>::failure(accountNamespace.error());
    }
    const auto key = cacheKey("youtube_data", url, "account_" + accountNamespace.value());
    std::optional<ServiceError> failure;
    auto authorization = oauth_->accessToken(cancellation);
    if (!authorization) {
        failure = authorization.error();
    } else {
        HttpRequest request;
        request.url = url;
        request.headers = {{"Accept", "application/json"},
                           {"Authorization", std::move(authorization).value()},
                           {"User-Agent", "BeatFlow/0.1"}};
        request.timeoutSeconds = 20;
        auto response = http_.send(request, cancellation);
        if (!response) {
            failure = response.error();
        } else if (response.value().status < 200 || response.value().status >= 300) {
            failure = httpError(response.value());
        } else {
            if (accountCache_ != nullptr) {
                const Json envelope{
                    {"schema", 1}, {"stored_at", nowEpochSeconds()}, {"body", response.value().body}};
                static_cast<void>(accountCache_->write(key, envelope.dump()));
            }
            return Outcome<RawResponse>::success({std::move(response.value().body), false});
        }
    }

    if (!cancellation.isCancellationRequested() && accountCache_ != nullptr) {
        auto cached = accountCache_->read(key);
        if (cached) {
            try {
                const auto envelope = Json::parse(cached.value());
                if (envelope.value("schema", 0) == 1 && envelope.contains("body") &&
                    envelope["body"].is_string()) {
                    return Outcome<RawResponse>::success({envelope["body"].get<std::string>(), true});
                }
            } catch (...) {
            }
            static_cast<void>(accountCache_->remove(key));
        }
    }
    return Outcome<RawResponse>::failure(*failure);
}

Outcome<std::vector<Track>> YouTubeMusicProvider::request(const std::string& endpoint,
                                                          const std::string& payload,
                                                          const std::string& source,
                                                          const CancellationToken& cancellation) {
    auto response = rawRequest(endpoint, payload, cancellation);
    if (!response) {
        return Outcome<std::vector<Track>>::failure(response.error());
    }
    auto parsed = parseTracks(response.value().body, source);
    if (parsed && response.value().stale) {
        for (auto& track : parsed.value()) {
            track.stale = true;
        }
    }
    return parsed;
}

std::string YouTubeMusicProvider::contextPayload(const std::string& clientVersion) const {
    Json context{{"context",
                  {{"client",
                    {{"clientName", "WEB_REMIX"},
                     {"clientVersion", clientVersion},
                     {"hl", language_},
                     {"gl", location_}}},
                   {"user", Json::object()}}}};
    return context.dump();
}

} // namespace beatflow
