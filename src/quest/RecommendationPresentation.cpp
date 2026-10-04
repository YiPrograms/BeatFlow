#include "beatnext/quest/RecommendationPresentation.hpp"

#include <cmath>
#include <iomanip>
#include <sstream>

namespace beatnext::quest::presentation {

std::string artists(const Track& track) {
    if (track.artists.empty())
        return "Unknown artist";
    std::ostringstream value;
    for (std::size_t index = 0; index < track.artists.size(); ++index) {
        if (index != 0)
            value << " · ";
        value << track.artists[index];
    }
    return value.str();
}

std::string duration(const RecommendedMap& recommendation) {
    const auto seconds = recommendation.track.durationSeconds ? recommendation.track.durationSeconds
                                                              : recommendation.map.durationSeconds;
    if (!seconds || *seconds <= 0)
        return {};
    std::ostringstream value;
    value << (*seconds / 60) << ':' << std::setfill('0') << std::setw(2) << (*seconds % 60);
    return value.str();
}

std::string mapMetadata(const RecommendedMap& recommendation) {
    std::ostringstream value;
    const auto length = duration(recommendation);
    if (!length.empty())
        value << length << " · ";
    value << "Mapped by " << recommendation.map.mapper << " · "
          << static_cast<int>(std::round(recommendation.map.rating * 100.0)) << '%';
    return value.str();
}

std::string difficulties(const RecommendedMap& recommendation) {
    std::ostringstream value;
    for (std::size_t index = 0; index < recommendation.playableDifficulties.size(); ++index) {
        if (index != 0)
            value << " · ";
        value << toString(recommendation.playableDifficulties[index].difficulty);
    }
    return value.str();
}

} // namespace beatnext::quest::presentation
