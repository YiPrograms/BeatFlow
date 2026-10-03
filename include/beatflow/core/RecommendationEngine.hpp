#pragma once

#include "beatflow/core/Interfaces.hpp"
#include "beatflow/core/Matcher.hpp"

#include <functional>
#include <memory>
#include <vector>

namespace beatflow {

class RecommendationEngine {
  public:
    using ProgressCallback = std::function<void(const RecommendedMap&)>;

    RecommendationEngine(MusicProvider& musicProvider, MapCatalog& mapCatalog, SongLibrary* songLibrary,
                         Matcher matcher = Matcher{});

    Outcome<std::vector<RecommendedMap>> forYou(const RecommendationRequest& request,
                                                const CancellationToken& cancellation,
                                                ProgressCallback onMatch = {});
    Outcome<std::vector<RecommendedMap>> following(const std::string& trackId,
                                                   const RecommendationRequest& request,
                                                   const CancellationToken& cancellation,
                                                   ProgressCallback onMatch = {});
    Outcome<std::vector<RecommendedMap>>
    upNext(const std::string& currentTitle, const std::string& currentArtist,
           std::optional<int> currentDurationSeconds, const RecommendationRequest& request,
           const CancellationToken& cancellation, ProgressCallback onMatch = {});
    Outcome<Track> resolveTrack(const std::string& title, const std::string& artist,
                                std::optional<int> durationSeconds, const CancellationToken& cancellation);

  private:
    Outcome<std::vector<RecommendedMap>> recommend(std::vector<Track> tracks,
                                                   const RecommendationRequest& request,
                                                   const CancellationToken& cancellation,
                                                   const ProgressCallback& onMatch);

    MusicProvider& musicProvider_;
    MapCatalog& mapCatalog_;
    SongLibrary* songLibrary_;
    Matcher matcher_;
};

} // namespace beatflow
