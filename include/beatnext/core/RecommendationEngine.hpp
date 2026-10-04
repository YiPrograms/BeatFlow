#pragma once

#include "beatnext/core/Interfaces.hpp"
#include "beatnext/core/Matcher.hpp"

#include <functional>
#include <memory>
#include <vector>

namespace beatnext {

class RecommendationEngine {
  public:
    using ProgressCallback = std::function<void(const RecommendedMap&)>;

    RecommendationEngine(MusicProvider& musicProvider, MapCatalog& mapCatalog, MapInstaller* mapInstaller,
                         Matcher matcher = Matcher{});

    Outcome<std::vector<RecommendedMap>> recommendAfter(const CurrentSong& currentSong,
                                                        const RecommendationRequest& request,
                                                        const CancellationToken& cancellation,
                                                        ProgressCallback onMatch = {});
    Outcome<Track> resolveTrack(const std::string& title, const std::string& artist,
                                std::optional<int> durationSeconds, const CancellationToken& cancellation);

  private:
    Outcome<std::vector<RecommendedMap>> following(const std::string& trackId,
                                                   const RecommendationRequest& request,
                                                   const CancellationToken& cancellation,
                                                   ProgressCallback onMatch);
    Outcome<std::vector<RecommendedMap>> recommend(std::vector<Track> tracks,
                                                   const RecommendationRequest& request,
                                                   const CancellationToken& cancellation,
                                                   const ProgressCallback& onMatch);

    MusicProvider& musicProvider_;
    MapCatalog& mapCatalog_;
    MapInstaller* mapInstaller_;
    Matcher matcher_;
};

} // namespace beatnext
