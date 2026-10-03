#pragma once

#include "beatflow/core/Interfaces.hpp"

namespace beatflow {

class BeatSaverCatalog final : public MapCatalog {
  public:
    explicit BeatSaverCatalog(HttpClient& http, CacheStore* cache = nullptr);

    Outcome<std::vector<MapCandidate>> search(const Track& track,
                                              const CancellationToken& cancellation) override;

    // Public for fixture-based contract tests. Production calls use search().
    Outcome<std::vector<MapCandidate>> parseSearchResponse(const std::string& response) const;

  private:
    HttpClient& http_;
    CacheStore* cache_;
};

} // namespace beatflow
