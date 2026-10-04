#pragma once

#include "beatnext/core/Models.hpp"
#include "beatnext/core/TextNormalizer.hpp"

#include <optional>
#include <vector>

namespace beatnext {

struct ScoringWeights {
    double titleIdentity{0.56};
    double artistIdentity{0.29};
    double durationIdentity{0.15};
    double minimumIdentity{0.62};
    double providerFinal{0.45};
    double qualityFinal{0.35};
    double suitabilityFinal{0.20};
};

class Matcher {
  public:
    explicit Matcher(ScoringWeights weights = {});

    [[nodiscard]] std::optional<RecommendedMap> evaluate(const Track& track, const MapCandidate& map) const;
    [[nodiscard]] std::vector<MapDifficulty> playableDifficulties(const MapCandidate& map) const;

  private:
    [[nodiscard]] double textSimilarity(const NormalizedText& left, const NormalizedText& right) const;
    [[nodiscard]] double artistSimilarity(const Track& track, const MapCandidate& map) const;
    [[nodiscard]] double durationSimilarity(const Track& track, const MapCandidate& map) const;
    [[nodiscard]] double quality(const MapCandidate& map) const;
    [[nodiscard]] double suitability(const std::vector<MapDifficulty>& difficulties) const;
    [[nodiscard]] bool recordingMarkersCompatible(const NormalizedText& track,
                                                  const NormalizedText& map) const;

    TextNormalizer normalizer_;
    ScoringWeights weights_;
};

} // namespace beatnext
