#pragma once

#include "beatnext/core/Interfaces.hpp"
#include "beatnext/core/Matcher.hpp"

#include <functional>
#include <memory>
#include <vector>

namespace beatnext {

class RecommendationEngine {
  public:
    using ProgressStage = RecommendationProgressStage;
    using Progress = RecommendationProgress;
    using ProgressCallback = std::function<void(const Progress&)>;

    RecommendationEngine(MusicProvider& musicProvider, MapCatalog& mapCatalog, MapInstaller* mapInstaller,
                         Matcher matcher = Matcher{});

    Outcome<std::vector<RecommendedMap>> recommendAfter(const CurrentSong& currentSong,
                                                        const RecommendationRequest& request,
                                                        const CancellationToken& cancellation,
                                                        ProgressCallback onProgress = {});
    Outcome<Track> resolveTrack(const std::string& title, const std::string& artist,
                                std::optional<int> durationSeconds, const CancellationToken& cancellation);

  private:
    Outcome<std::vector<RecommendedMap>> following(const Track& sourceTrack,
                                                   const RecommendationRequest& request,
                                                   const CancellationToken& cancellation,
                                                   ProgressCallback onProgress);
    Outcome<std::vector<RecommendedMap>> recommend(std::vector<Track> tracks,
                                                   const RecommendationRequest& request,
                                                   const CancellationToken& cancellation,
                                                   const ProgressCallback& onProgress,
                                                   const Track& sourceTrack);

    MusicProvider& musicProvider_;
    MapCatalog& mapCatalog_;
    MapInstaller* mapInstaller_;
    Matcher matcher_;
};

} // namespace beatnext
