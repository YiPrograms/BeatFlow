#pragma once

#include "beatnext/core/Interfaces.hpp"
#include "beatnext/services/BeatSaverCatalog.hpp"

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace SongDetailsCache {
class SongDetails;
}

namespace beatnext::quest {

// Uses the same complete on-device BeatSaver catalog as BetterSongSearch.
// The live BeatSaver text endpoint remains a fallback while the catalog loads.
class SongDetailsCatalog final : public MapCatalog {
  public:
    explicit SongDetailsCatalog(BeatSaverCatalog& fallback);

    Outcome<std::vector<MapCandidate>> search(const Track& track,
                                              const CancellationToken& cancellation) override;

  private:
    bool ensureIndex();

    BeatSaverCatalog& fallback_;
    std::mutex mutex_;
    SongDetailsCache::SongDetails* details_{nullptr};
    std::unordered_map<std::string, std::vector<std::uint32_t>> titleTokenIndex_;
    bool initialized_{false};
};

} // namespace beatnext::quest
