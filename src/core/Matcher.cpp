#include "beatflow/core/Matcher.hpp"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <numeric>
#include <set>
#include <string>
#include <string_view>

namespace beatflow {
namespace {

double clamp01(double value) {
    return std::clamp(value, 0.0, 1.0);
}

double normalizedEditSimilarity(const std::string& left, const std::string& right) {
    if (left == right) {
        return 1.0;
    }
    if (left.empty() || right.empty()) {
        return 0.0;
    }

    std::vector<std::size_t> previous(right.size() + 1);
    std::vector<std::size_t> current(right.size() + 1);
    std::iota(previous.begin(), previous.end(), std::size_t{0});

    for (std::size_t row = 1; row <= left.size(); ++row) {
        current[0] = row;
        for (std::size_t column = 1; column <= right.size(); ++column) {
            const std::size_t substitution = left[row - 1] == right[column - 1] ? 0 : 1;
            current[column] = std::min(
                {previous[column] + 1, current[column - 1] + 1, previous[column - 1] + substitution});
        }
        std::swap(previous, current);
    }

    const auto maximumLength = static_cast<double>(std::max(left.size(), right.size()));
    return 1.0 - static_cast<double>(previous.back()) / maximumLength;
}

double tokenDice(const std::vector<std::string>& left, const std::vector<std::string>& right) {
    if (left.empty() || right.empty()) {
        return 0.0;
    }
    std::multiset<std::string> leftSet(left.begin(), left.end());
    std::multiset<std::string> rightSet(right.begin(), right.end());
    std::vector<std::string> intersection;
    std::set_intersection(leftSet.begin(), leftSet.end(), rightSet.begin(), rightSet.end(),
                          std::back_inserter(intersection));
    return (2.0 * static_cast<double>(intersection.size())) / static_cast<double>(left.size() + right.size());
}

std::vector<std::string_view> titleAliases(std::string_view title) {
    std::vector<std::string_view> aliases{title};
    for (const auto separator : {std::string_view{" - "}, std::string_view{" / "}, std::string_view{" | "}}) {
        std::size_t start = 0;
        auto position = title.find(separator, start);
        while (position != std::string_view::npos) {
            if (position > start) {
                aliases.push_back(title.substr(start, position - start));
            }
            start = position + separator.size();
            if (start < title.size()) {
                aliases.push_back(title.substr(start));
            }
            position = title.find(separator, start);
        }
    }
    return aliases;
}

} // namespace

Matcher::Matcher(ScoringWeights weights) : weights_(weights) {}

std::optional<RecommendedMap> Matcher::evaluate(const Track& track, const MapCandidate& map,
                                                const RecommendationFilters& filters) const {
    const auto playable = playableDifficulties(map, filters);
    if (playable.empty() || map.automapper) {
        return std::nullopt;
    }

    const auto normalizedMapTitle = normalizer_.title(map.songTitle);
    double bestTitleScore = 0.0;
    bool compatibleAlias = false;
    for (const auto alias : titleAliases(track.title)) {
        const auto normalizedAlias = normalizer_.title(alias);
        if (!recordingMarkersCompatible(normalizedAlias, normalizedMapTitle)) {
            continue;
        }
        compatibleAlias = true;
        bestTitleScore = std::max(bestTitleScore, textSimilarity(normalizedAlias, normalizedMapTitle));
    }
    if (!compatibleAlias) {
        return std::nullopt;
    }

    MatchScores scores;
    scores.title = bestTitleScore;
    scores.artist = artistSimilarity(track, map);
    scores.duration = durationSimilarity(track, map);
    scores.identity = scores.title * weights_.titleIdentity + scores.artist * weights_.artistIdentity +
                      scores.duration * weights_.durationIdentity;
    if (scores.identity < weights_.minimumIdentity || scores.title < 0.48) {
        return std::nullopt;
    }

    scores.quality = quality(map);
    scores.suitability = suitability(playable, filters);
    scores.finalScore = clamp01(track.providerRelevance) * weights_.providerFinal +
                        scores.quality * weights_.qualityFinal +
                        scores.suitability * weights_.suitabilityFinal;

    return RecommendedMap{track, map, scores, playable, false};
}

std::vector<MapDifficulty> Matcher::playableDifficulties(const MapCandidate& map,
                                                         const RecommendationFilters& filters) const {
    std::vector<MapDifficulty> result;
    std::copy_if(map.difficulties.begin(), map.difficulties.end(), std::back_inserter(result),
                 [&filters](const MapDifficulty& difficulty) {
                     if (filters.standardOnly && difficulty.characteristic != "Standard") {
                         return false;
                     }
                     if (!filters.difficulties.empty() &&
                         !filters.difficulties.contains(difficulty.difficulty)) {
                         return false;
                     }
                     if (filters.minimumNps && difficulty.notesPerSecond < *filters.minimumNps) {
                         return false;
                     }
                     if (filters.maximumNps && difficulty.notesPerSecond > *filters.maximumNps) {
                         return false;
                     }
                     return difficulty.requirements.empty();
                 });
    return result;
}

double Matcher::textSimilarity(const NormalizedText& left, const NormalizedText& right) const {
    return clamp01(tokenDice(left.tokens, right.tokens) * 0.65 +
                   normalizedEditSimilarity(left.text, right.text) * 0.35);
}

double Matcher::artistSimilarity(const Track& track, const MapCandidate& map) const {
    if (track.artists.empty() || map.songArtist.empty()) {
        return 0.55;
    }

    const auto mapArtist = normalizer_.artist(map.songArtist);
    double best = 0.0;
    for (const auto& artist : track.artists) {
        best = std::max(best, textSimilarity(normalizer_.artist(artist), mapArtist));
    }
    return best;
}

double Matcher::durationSimilarity(const Track& track, const MapCandidate& map) const {
    if (!track.durationSeconds || !map.durationSeconds) {
        return 0.65;
    }
    const int difference = std::abs(*track.durationSeconds - *map.durationSeconds);
    if (difference <= 2) {
        return 1.0;
    }
    return std::exp(-static_cast<double>(difference - 2) / 24.0);
}

double Matcher::quality(const MapCandidate& map) const {
    const auto votes = static_cast<double>(map.upvotes + map.downvotes);
    const double confidence = 1.0 - std::exp(-votes / 50.0);
    double result = clamp01(map.rating) * confidence + 0.5 * (1.0 - confidence);
    if (map.curated) {
        result += 0.04;
    }
    return clamp01(result);
}

double Matcher::suitability(const std::vector<MapDifficulty>& difficulties,
                            const RecommendationFilters& filters) const {
    if (difficulties.empty()) {
        return 0.0;
    }
    if (!filters.minimumNps && !filters.maximumNps && filters.difficulties.empty()) {
        return std::min(1.0, 0.72 + static_cast<double>(difficulties.size()) * 0.07);
    }

    if (filters.minimumNps && filters.maximumNps) {
        const double center = (*filters.minimumNps + *filters.maximumNps) / 2.0;
        const double halfRange = std::max(0.5, (*filters.maximumNps - *filters.minimumNps) / 2.0);
        double best = 0.0;
        for (const auto& difficulty : difficulties) {
            best = std::max(best, 1.0 - std::abs(difficulty.notesPerSecond - center) / (halfRange * 2.0));
        }
        return clamp01(best);
    }
    return 1.0;
}

bool Matcher::recordingMarkersCompatible(const NormalizedText& track, const NormalizedText& map) const {
    return track.recordingMarkers == map.recordingMarkers;
}

} // namespace beatflow
