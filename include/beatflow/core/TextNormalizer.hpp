#pragma once

#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace beatflow {

struct NormalizedText {
    std::string text;
    std::vector<std::string> tokens;
    std::set<std::string> recordingMarkers;
};

class TextNormalizer {
  public:
    [[nodiscard]] NormalizedText title(std::string_view input) const;
    [[nodiscard]] NormalizedText artist(std::string_view input) const;
    [[nodiscard]] static std::string searchQuery(std::string_view title, std::string_view artist);

  private:
    [[nodiscard]] NormalizedText normalize(std::string_view input, bool stripPresentationNoise) const;
};

} // namespace beatflow
