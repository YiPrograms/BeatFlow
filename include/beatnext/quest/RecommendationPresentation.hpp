#pragma once

#include "beatnext/core/Models.hpp"

#include <string>

namespace beatnext::quest::presentation {

[[nodiscard]] std::string artists(const Track& track);
[[nodiscard]] std::string duration(const RecommendedMap& recommendation);
[[nodiscard]] std::string mapMetadata(const RecommendedMap& recommendation);
[[nodiscard]] std::string difficulties(const RecommendedMap& recommendation);

} // namespace beatnext::quest::presentation
