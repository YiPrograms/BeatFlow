#include "beatnext/core/TextNormalizer.hpp"

#include "beatnext/core/Models.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <unordered_set>

namespace beatnext {
namespace {

const std::vector<std::pair<std::string, std::string>> kMarkers{
    {"remix", "remix"},
    {"re mix", "remix"},
    {"cover", "cover"},
    {"live", "live"},
    {"nightcore", "nightcore"},
    {"sped up", "sped-up"},
    {"slowed", "slowed"},
    {"tv size", "short"},
    {"tv edit", "short"},
    {"radio edit", "short"},
    {"short ver", "short"},
    {"short version", "short"},
    {"instrumental", "instrumental"},
};

const std::unordered_set<std::string> kNoiseTokens{
    "official", "audio", "video", "lyrics", "lyric", "mv", "m v", "visualizer",
};

std::string collapseSpaces(std::string value) {
    std::string result;
    bool previousSpace = true;
    for (char character : value) {
        const bool isSpace = character == ' ';
        if (isSpace && previousSpace) {
            continue;
        }
        result.push_back(character);
        previousSpace = isSpace;
    }
    if (!result.empty() && result.back() == ' ') {
        result.pop_back();
    }
    return result;
}

std::vector<std::string> splitTokens(const std::string& value) {
    std::istringstream stream(value);
    std::vector<std::string> result;
    for (std::string token; stream >> token;) {
        result.push_back(std::move(token));
    }
    return result;
}

void replaceAll(std::string& value, std::string_view needle, char replacement) {
    std::size_t position = 0;
    while ((position = value.find(needle, position)) != std::string::npos) {
        value.replace(position, needle.size(), 1, replacement);
        ++position;
    }
}

} // namespace

std::string toString(Difficulty difficulty) {
    switch (difficulty) {
    case Difficulty::Easy:
        return "Easy";
    case Difficulty::Normal:
        return "Normal";
    case Difficulty::Hard:
        return "Hard";
    case Difficulty::Expert:
        return "Expert";
    case Difficulty::ExpertPlus:
        return "ExpertPlus";
    }
    return "Unknown";
}

std::optional<Difficulty> difficultyFromString(const std::string& value) {
    if (value == "Easy") {
        return Difficulty::Easy;
    }
    if (value == "Normal") {
        return Difficulty::Normal;
    }
    if (value == "Hard") {
        return Difficulty::Hard;
    }
    if (value == "Expert") {
        return Difficulty::Expert;
    }
    if (value == "ExpertPlus" || value == "Expert+") {
        return Difficulty::ExpertPlus;
    }
    return std::nullopt;
}

NormalizedText TextNormalizer::title(std::string_view input) const {
    return normalize(input, true);
}

NormalizedText TextNormalizer::artist(std::string_view input) const {
    return normalize(input, false);
}

std::string TextNormalizer::searchQuery(std::string_view title, std::string_view artist) {
    std::string result;
    result.reserve(title.size() + artist.size() + 1);
    result.append(artist);
    if (!result.empty() && !title.empty()) {
        result.push_back(' ');
    }
    result.append(title);
    return result;
}

NormalizedText TextNormalizer::normalize(std::string_view input, bool stripPresentationNoise) const {
    std::string cleaned(input);
    for (const auto punctuation : {"【", "】", "「", "」", "『", "』", "（", "）", "〈", "〉", "《", "》",
                                   "［", "］", "・", "：", "—", "–"}) {
        replaceAll(cleaned, punctuation, ' ');
    }
    std::string normalized;
    normalized.reserve(cleaned.size());

    for (const unsigned char character : cleaned) {
        if (character >= 0x80U) {
            normalized.push_back(static_cast<char>(character));
            continue;
        }
        if (std::isalnum(character) != 0) {
            normalized.push_back(static_cast<char>(std::tolower(character)));
        } else {
            normalized.push_back(' ');
        }
    }
    normalized = collapseSpaces(std::move(normalized));

    NormalizedText result;
    for (const auto& [needle, marker] : kMarkers) {
        if (normalized.find(needle) != std::string::npos) {
            result.recordingMarkers.insert(marker);
        }
    }

    auto tokens = splitTokens(normalized);
    if (stripPresentationNoise) {
        tokens.erase(std::remove_if(tokens.begin(), tokens.end(),
                                    [](const std::string& token) { return kNoiseTokens.contains(token); }),
                     tokens.end());
    }

    std::ostringstream joined;
    for (std::size_t index = 0; index < tokens.size(); ++index) {
        if (index != 0) {
            joined << ' ';
        }
        joined << tokens[index];
    }
    result.text = joined.str();
    result.tokens = std::move(tokens);
    return result;
}

} // namespace beatnext
