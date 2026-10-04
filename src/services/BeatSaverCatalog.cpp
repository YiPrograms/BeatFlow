#include "beatnext/services/BeatSaverCatalog.hpp"

#include "beatnext/core/TextNormalizer.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace beatnext {
namespace {

using Json = nlohmann::json;

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

std::string cacheKey(const std::string& value) {
    std::uint64_t hash = 1469598103934665603ULL;
    for (const unsigned char character : value) {
        hash ^= character;
        hash *= 1099511628211ULL;
    }
    std::ostringstream key;
    key << "beatsaver_" << std::hex << hash;
    return key.str();
}

std::int64_t nowEpochSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

std::vector<std::string> requirements(const Json& difficulty) {
    std::vector<std::string> result;
    const auto explicitRequirements = difficulty.find("requirements");
    if (explicitRequirements != difficulty.end() && explicitRequirements->is_array()) {
        for (const auto& requirement : *explicitRequirements) {
            if (requirement.is_string()) {
                result.push_back(requirement.get<std::string>());
            }
        }
    }
    if (difficulty.value("ne", false)) {
        result.emplace_back("Noodle Extensions");
    }
    if (difficulty.value("me", false)) {
        result.emplace_back("Mapping Extensions");
    }
    if (difficulty.value("cinema", false)) {
        result.emplace_back("Cinema");
    }
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

ServiceError responseError(const HttpResponse& response) {
    if (response.status == 429) {
        return {ErrorCode::RateLimited, "BeatSaver rate-limited the request.", true, std::nullopt};
    }
    return {ErrorCode::Network, "BeatSaver returned HTTP " + std::to_string(response.status) + ".",
            response.status >= 500, std::nullopt};
}

} // namespace

BeatSaverCatalog::BeatSaverCatalog(HttpClient& http, CacheStore* cache) : http_(http), cache_(cache) {}

Outcome<std::vector<MapCandidate>> BeatSaverCatalog::search(const Track& track,
                                                            const CancellationToken& cancellation) {
    if (cancellation.isCancellationRequested()) {
        return Outcome<std::vector<MapCandidate>>::failure(
            {ErrorCode::Cancelled, "The BeatSaver search was cancelled.", false, std::nullopt});
    }
    const auto query =
        TextNormalizer::searchQuery(track.title, track.artists.empty() ? "" : track.artists.front());
    const auto key = cacheKey(query);
    HttpRequest request;
    request.url = "https://api.beatsaver.com/search/text/0?q=" + urlEncode(query) +
                  "&sortOrder=Relevance&automapper=false";
    request.headers = {{"Accept", "application/json"}, {"User-Agent", "BeatNext/0.1"}};
    request.timeoutSeconds = 20;
    auto response = http_.send(request, cancellation);
    if (response && response.value().status >= 200 && response.value().status < 300) {
        auto parsed = parseSearchResponse(response.value().body);
        if (parsed && cache_ != nullptr) {
            const Json envelope{
                {"schema", 1}, {"stored_at", nowEpochSeconds()}, {"body", response.value().body}};
            static_cast<void>(cache_->write(key, envelope.dump()));
        }
        return parsed;
    }

    if (cache_ != nullptr) {
        auto cached = cache_->read(key);
        if (cached) {
            try {
                const auto envelope = Json::parse(cached.value());
                if (envelope.value("schema", 0) == 1 && envelope.contains("body") &&
                    envelope["body"].is_string()) {
                    auto parsed = parseSearchResponse(envelope["body"].get<std::string>());
                    if (parsed) {
                        for (auto& map : parsed.value()) {
                            map.stale = true;
                        }
                        return parsed;
                    }
                }
            } catch (...) {
            }
            static_cast<void>(cache_->remove(key));
        }
    }
    if (!response) {
        return Outcome<std::vector<MapCandidate>>::failure(response.error());
    }
    return Outcome<std::vector<MapCandidate>>::failure(responseError(response.value()));
}

Outcome<std::vector<MapCandidate>> BeatSaverCatalog::parseSearchResponse(const std::string& response) const {
    try {
        const auto root = Json::parse(response);
        if (!root.contains("docs") || !root["docs"].is_array()) {
            return Outcome<std::vector<MapCandidate>>::failure(
                {ErrorCode::InvalidResponse, "BeatSaver response did not contain a docs array.", false,
                 std::nullopt});
        }

        std::vector<MapCandidate> maps;
        for (const auto& document : root["docs"]) {
            if (!document.is_object() || !document.contains("metadata") || !document.contains("versions")) {
                continue;
            }
            const auto& metadata = document["metadata"];
            const Json* version = nullptr;
            for (const auto& candidateVersion : document["versions"]) {
                if (candidateVersion.is_object() &&
                    candidateVersion.value("state", "Published") == "Published") {
                    version = &candidateVersion;
                    break;
                }
            }
            if (version == nullptr) {
                continue;
            }

            MapCandidate map;
            map.key = document.value("id", "");
            map.hash = version->value("hash", "");
            map.songTitle = metadata.value("songName", document.value("name", ""));
            map.songArtist = metadata.value("songAuthorName", "");
            map.mapper = metadata.value("levelAuthorName", "");
            if (metadata.contains("duration") && metadata["duration"].is_number_integer()) {
                map.durationSeconds = metadata["duration"].get<int>();
            }
            if (document.contains("stats") && document["stats"].is_object()) {
                map.rating = document["stats"].value("score", 0.0);
                map.upvotes = document["stats"].value("upvotes", 0U);
                map.downvotes = document["stats"].value("downvotes", 0U);
            }
            map.curated = document.contains("curatedAt") && !document["curatedAt"].is_null();
            map.automapper =
                document.value("automapper", false) || document.value("declaredAi", "None") != "None";
            map.coverUrl = version->value("coverURL", "");
            map.downloadUrl = version->value("downloadURL", "");

            if (version->contains("diffs") && (*version)["diffs"].is_array()) {
                for (const auto& rawDifficulty : (*version)["diffs"]) {
                    const auto parsedDifficulty = difficultyFromString(rawDifficulty.value("difficulty", ""));
                    if (!parsedDifficulty) {
                        continue;
                    }
                    map.difficulties.push_back(
                        {*parsedDifficulty, rawDifficulty.value("characteristic", "Standard"),
                         rawDifficulty.value("nps", 0.0), requirements(rawDifficulty)});
                }
            }
            if (!map.key.empty() && !map.hash.empty() && !map.songTitle.empty()) {
                maps.push_back(std::move(map));
            }
        }
        return Outcome<std::vector<MapCandidate>>::success(std::move(maps));
    } catch (const std::exception& exception) {
        return Outcome<std::vector<MapCandidate>>::failure(
            {ErrorCode::InvalidResponse, std::string("BeatSaver returned invalid JSON: ") + exception.what(),
             false, std::nullopt});
    }
}

} // namespace beatnext
