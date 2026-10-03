#include "beatflow/core/RecommendationEngine.hpp"

#include "beatflow/core/TextNormalizer.hpp"

#include <algorithm>
#include <cctype>
#include <unordered_set>

namespace beatflow {
namespace {

ServiceError cancelledError() {
    return {ErrorCode::Cancelled, "The recommendation request was cancelled.", false, std::nullopt};
}

std::string lowercaseAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return value;
}

} // namespace

RecommendationEngine::RecommendationEngine(MusicProvider& musicProvider, MapCatalog& mapCatalog,
                                           SongLibrary* songLibrary, Matcher matcher)
    : musicProvider_(musicProvider), mapCatalog_(mapCatalog), songLibrary_(songLibrary),
      matcher_(std::move(matcher)) {}

Outcome<std::vector<RecommendedMap>> RecommendationEngine::forYou(const RecommendationRequest& request,
                                                                  const CancellationToken& cancellation,
                                                                  ProgressCallback onMatch) {
    auto tracks = musicProvider_.home(cancellation);
    if (!tracks) {
        return Outcome<std::vector<RecommendedMap>>::failure(tracks.error());
    }
    return recommend(std::move(tracks).value(), request, cancellation, onMatch);
}

Outcome<std::vector<RecommendedMap>> RecommendationEngine::following(const std::string& trackId,
                                                                     const RecommendationRequest& request,
                                                                     const CancellationToken& cancellation,
                                                                     ProgressCallback onMatch) {
    auto tracks = musicProvider_.radio(trackId, cancellation);
    if (!tracks) {
        return Outcome<std::vector<RecommendedMap>>::failure(tracks.error());
    }
    auto updatedRequest = request;
    updatedRequest.currentTrackId = trackId;
    return recommend(std::move(tracks).value(), updatedRequest, cancellation, onMatch);
}

Outcome<std::vector<RecommendedMap>>
RecommendationEngine::upNext(const std::string& currentTitle, const std::string& currentArtist,
                             std::optional<int> currentDurationSeconds, const RecommendationRequest& request,
                             const CancellationToken& cancellation, ProgressCallback onMatch) {
    auto currentTrack = resolveTrack(currentTitle, currentArtist, currentDurationSeconds, cancellation);
    if (!currentTrack) {
        return Outcome<std::vector<RecommendedMap>>::failure(currentTrack.error());
    }
    return following(currentTrack.value().providerId, request, cancellation, std::move(onMatch));
}

Outcome<Track> RecommendationEngine::resolveTrack(const std::string& title, const std::string& artist,
                                                  std::optional<int> durationSeconds,
                                                  const CancellationToken& cancellation) {
    auto candidates = musicProvider_.search(TextNormalizer::searchQuery(title, artist), cancellation);
    if (!candidates) {
        return Outcome<Track>::failure(candidates.error());
    }

    Track target{"",
                 title,
                 artist.empty() ? std::vector<std::string>{} : std::vector<std::string>{artist},
                 durationSeconds,
                 "",
                 "",
                 1.0};
    std::optional<Track> best;
    double bestIdentity = 0.0;
    for (const auto& candidate : candidates.value()) {
        MapCandidate comparable;
        comparable.songTitle = candidate.title;
        comparable.songArtist = candidate.artists.empty() ? "" : candidate.artists.front();
        comparable.durationSeconds = candidate.durationSeconds;
        comparable.rating = 1.0;
        comparable.upvotes = 100;
        comparable.difficulties.push_back({Difficulty::Easy, "Standard", 1.0, {}});
        auto result = matcher_.evaluate(target, comparable, {});
        if (result && result->scores.identity > bestIdentity) {
            bestIdentity = result->scores.identity;
            best = candidate;
        }
    }

    if (!best) {
        return Outcome<Track>::failure({ErrorCode::NotFound,
                                        "YouTube Music could not confidently identify this song.", false,
                                        std::nullopt});
    }
    return Outcome<Track>::success(std::move(*best));
}

Outcome<std::vector<RecommendedMap>> RecommendationEngine::recommend(std::vector<Track> tracks,
                                                                     const RecommendationRequest& request,
                                                                     const CancellationToken& cancellation,
                                                                     const ProgressCallback& onMatch) {
    std::vector<RecommendedMap> recommendations;
    std::unordered_set<std::string> seenTracks;
    std::unordered_set<std::string> seenHashes;

    const auto trackLimit = std::min(request.maximumTracks, tracks.size());
    for (std::size_t index = 0; index < trackLimit; ++index) {
        if (cancellation.isCancellationRequested()) {
            return Outcome<std::vector<RecommendedMap>>::failure(cancelledError());
        }

        auto& track = tracks[index];
        if (track.providerId.empty() ||
            (request.currentTrackId && track.providerId == *request.currentTrackId) ||
            !seenTracks.insert(track.providerId).second) {
            continue;
        }
        if (track.providerRelevance <= 0.0 || track.providerRelevance > 1.0) {
            track.providerRelevance = 1.0 - (static_cast<double>(index) /
                                             static_cast<double>(std::max<std::size_t>(trackLimit, 1))) *
                                                0.45;
        }

        auto maps = mapCatalog_.search(track, cancellation);
        if (!maps) {
            if (maps.error().code == ErrorCode::Cancelled) {
                return Outcome<std::vector<RecommendedMap>>::failure(maps.error());
            }
            continue;
        }

        std::optional<RecommendedMap> best;
        for (const auto& map : maps.value()) {
            const auto normalizedHash = lowercaseAscii(map.hash);
            if (map.hash.empty() || request.excludedMapHashes.contains(normalizedHash) ||
                seenHashes.contains(normalizedHash)) {
                continue;
            }
            auto candidate = matcher_.evaluate(track, map, request.filters);
            if (!candidate) {
                continue;
            }
            candidate->installed = songLibrary_ != nullptr && songLibrary_->isInstalled(map.hash);
            if (!best || candidate->scores.finalScore > best->scores.finalScore) {
                best = std::move(candidate);
            }
        }

        if (best) {
            seenHashes.insert(lowercaseAscii(best->map.hash));
            recommendations.push_back(std::move(*best));
            if (onMatch) {
                onMatch(recommendations.back());
            }
        }

        if (recommendations.size() >= request.maximumResults) {
            break;
        }
    }

    std::stable_sort(recommendations.begin(), recommendations.end(),
                     [](const RecommendedMap& left, const RecommendedMap& right) {
                         return left.scores.finalScore > right.scores.finalScore;
                     });
    return Outcome<std::vector<RecommendedMap>>::success(std::move(recommendations));
}

} // namespace beatflow
