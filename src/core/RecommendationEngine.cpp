#include "beatnext/core/RecommendationEngine.hpp"

#include "beatnext/core/TextNormalizer.hpp"

#include <algorithm>
#include <cctype>
#include <unordered_set>

namespace beatnext {
namespace {

ServiceError cancelledError() {
    return {ErrorCode::Cancelled, "The recommendation request was cancelled.", false, std::nullopt};
}

std::string lowercaseAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return value;
}

std::string stripShortVersionLabel(std::string value) {
    auto lowercase = lowercaseAscii(value);
    for (const std::string_view label : {"short version", "short ver", "tv size", "tv edit", "radio edit"}) {
        std::size_t position = 0;
        while ((position = lowercase.find(label, position)) != std::string::npos) {
            value.replace(position, label.size(), label.size(), ' ');
            lowercase.replace(position, label.size(), label.size(), ' ');
            position += label.size();
        }
    }
    std::string collapsed;
    collapsed.reserve(value.size());
    bool previousSpace = true;
    for (const unsigned char character : value) {
        const bool space = std::isspace(character) != 0;
        if (!space || !previousSpace) {
            collapsed.push_back(space ? ' ' : static_cast<char>(character));
        }
        previousSpace = space;
    }
    if (!collapsed.empty() && collapsed.back() == ' ') {
        collapsed.pop_back();
    }
    for (const std::string_view emptyPair : {"()", "( )", "[]", "[ ]", "{}", "{ }"}) {
        std::size_t position = 0;
        while ((position = collapsed.find(emptyPair, position)) != std::string::npos) {
            collapsed.erase(position, emptyPair.size());
        }
    }
    while (!collapsed.empty() && collapsed.back() == ' ') {
        collapsed.pop_back();
    }
    return collapsed;
}

} // namespace

RecommendationEngine::RecommendationEngine(MusicProvider& musicProvider, MapCatalog& mapCatalog,
                                           MapInstaller* mapInstaller, Matcher matcher)
    : musicProvider_(musicProvider), mapCatalog_(mapCatalog), mapInstaller_(mapInstaller),
      matcher_(std::move(matcher)) {}

Outcome<std::vector<RecommendedMap>> RecommendationEngine::following(const Track& sourceTrack,
                                                                     const RecommendationRequest& request,
                                                                     const CancellationToken& cancellation,
                                                                     ProgressCallback onProgress) {
    if (onProgress) {
        Progress progress;
        progress.stage = ProgressStage::LoadingRadio;
        progress.sourceTrack = sourceTrack;
        onProgress(progress);
    }
    auto tracks = musicProvider_.radio(sourceTrack.providerId, cancellation);
    if (!tracks) {
        return Outcome<std::vector<RecommendedMap>>::failure(tracks.error());
    }
    auto updatedRequest = request;
    updatedRequest.currentTrackId = sourceTrack.providerId;
    return recommend(std::move(tracks).value(), updatedRequest, cancellation, onProgress, sourceTrack);
}

Outcome<std::vector<RecommendedMap>>
RecommendationEngine::recommendAfter(const CurrentSong& currentSong, const RecommendationRequest& request,
                                     const CancellationToken& cancellation, ProgressCallback onProgress) {
    if (cancellation.isCancellationRequested()) {
        return Outcome<std::vector<RecommendedMap>>::failure(cancelledError());
    }
    if (onProgress) {
        Progress progress;
        progress.stage = ProgressStage::ResolvingCurrentSong;
        onProgress(progress);
    }
    auto currentTrack =
        resolveTrack(currentSong.title, currentSong.artist, currentSong.durationSeconds, cancellation);
    if (!currentTrack) {
        return Outcome<std::vector<RecommendedMap>>::failure(currentTrack.error());
    }
    return following(currentTrack.value(), request, cancellation, std::move(onProgress));
}

Outcome<Track> RecommendationEngine::resolveTrack(const std::string& title, const std::string& artist,
                                                  std::optional<int> durationSeconds,
                                                  const CancellationToken& cancellation) {
    auto exactCandidates = musicProvider_.search(TextNormalizer::searchQuery(title, artist), cancellation);
    if (!exactCandidates) {
        return Outcome<Track>::failure(exactCandidates.error());
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
    const auto selectBest = [this, &best, &bestIdentity](const Track& identityTarget,
                                                         const std::vector<Track>& candidates) {
        for (const auto& candidate : candidates) {
            MapCandidate comparable;
            comparable.songTitle = candidate.title;
            comparable.songArtist = candidate.artists.empty() ? "" : candidate.artists.front();
            comparable.durationSeconds = candidate.durationSeconds;
            comparable.rating = 1.0;
            comparable.upvotes = 100;
            comparable.difficulties.push_back({Difficulty::Easy, "Standard", 1.0, {}});
            auto result = matcher_.evaluate(identityTarget, comparable);
            if (result && result->scores.identity > bestIdentity) {
                bestIdentity = result->scores.identity;
                best = candidate;
            }
        }
    };
    selectBest(target, exactCandidates.value());

    std::optional<Track> providerBest;
    if (!exactCandidates.value().empty()) {
        providerBest = exactCandidates.value().front();
    }

    // Short edits are frequently absent from YouTube Music even when the original
    // recording is present. Using the same recording as the radio seed is safe
    // when title and artist still match; duration is intentionally neutral here.
    const auto normalizedTitle = TextNormalizer{}.title(title);
    if (normalizedTitle.recordingMarkers.contains("short")) {
        auto originalRecording = target;
        originalRecording.title = stripShortVersionLabel(title);
        originalRecording.durationSeconds.reset();
        auto originalCandidates =
            musicProvider_.search(TextNormalizer::searchQuery(originalRecording.title, artist), cancellation);
        if (originalCandidates) {
            selectBest(originalRecording, originalCandidates.value());
            if (!originalCandidates.value().empty()) {
                providerBest = originalCandidates.value().front();
            }
        } else if (!providerBest) {
            return Outcome<Track>::failure(originalCandidates.error());
        }
    }

    if (best) {
        return Outcome<Track>::success(std::move(*best));
    }
    if (providerBest) {
        // Search results already carry the provider's relevance ordering. A loose
        // fallback is appropriate for a radio seed; BeatSaver map selection still
        // applies the strict recording-identity matcher below this boundary.
        return Outcome<Track>::success(std::move(*providerBest));
    }
    return Outcome<Track>::failure(
        {ErrorCode::NotFound, "YouTube Music search returned no tracks for this song.", false, std::nullopt});
}

Outcome<std::vector<RecommendedMap>> RecommendationEngine::recommend(std::vector<Track> tracks,
                                                                     const RecommendationRequest& request,
                                                                     const CancellationToken& cancellation,
                                                                     const ProgressCallback& onProgress,
                                                                     const Track& sourceTrack) {
    std::vector<RecommendedMap> recommendations;
    std::unordered_set<std::string> seenTracks;
    std::unordered_set<std::string> seenHashes;

    const auto trackLimit = std::min(request.maximumTracks, tracks.size());
    const auto publishProgress = [&](std::size_t completed) {
        if (onProgress) {
            onProgress(
                {ProgressStage::MatchingMaps, sourceTrack, completed, trackLimit, recommendations.size()});
        }
    };
    publishProgress(0);
    for (std::size_t index = 0; index < trackLimit; ++index) {
        if (cancellation.isCancellationRequested()) {
            return Outcome<std::vector<RecommendedMap>>::failure(cancelledError());
        }

        auto& track = tracks[index];
        if (track.providerId.empty() ||
            (request.currentTrackId && track.providerId == *request.currentTrackId) ||
            !seenTracks.insert(track.providerId).second) {
            publishProgress(index + 1);
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
            publishProgress(index + 1);
            continue;
        }

        std::optional<RecommendedMap> best;
        for (const auto& map : maps.value()) {
            const auto normalizedHash = lowercaseAscii(map.hash);
            if (map.hash.empty() || request.excludedMapHashes.contains(normalizedHash) ||
                seenHashes.contains(normalizedHash)) {
                continue;
            }
            auto candidate = matcher_.evaluate(track, map);
            if (!candidate) {
                continue;
            }
            candidate->installed = mapInstaller_ != nullptr && mapInstaller_->isInstalled(map.hash);
            if (!best || candidate->scores.finalScore > best->scores.finalScore) {
                best = std::move(candidate);
            }
        }

        if (best) {
            seenHashes.insert(lowercaseAscii(best->map.hash));
            recommendations.push_back(std::move(*best));
        }
        publishProgress(index + 1);

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

} // namespace beatnext
