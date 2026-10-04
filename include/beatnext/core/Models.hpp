#pragma once

#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace beatnext {

enum class Difficulty { Easy, Normal, Hard, Expert, ExpertPlus };

[[nodiscard]] std::string toString(Difficulty difficulty);
[[nodiscard]] std::optional<Difficulty> difficultyFromString(const std::string& value);

struct Track {
    std::string providerId;
    std::string title;
    std::vector<std::string> artists;
    std::optional<int> durationSeconds;
    std::string artworkUrl;
    std::string sourceShelf;
    double providerRelevance{0.5};
    bool stale{false};
};

enum class RecommendationProgressStage { ResolvingCurrentSong, LoadingRadio, MatchingMaps };

struct RecommendationProgress {
    RecommendationProgressStage stage{RecommendationProgressStage::ResolvingCurrentSong};
    std::optional<Track> sourceTrack;
    std::size_t completedTracks{0};
    std::size_t totalTracks{0};
    std::size_t matchesFound{0};
};

struct CurrentSong {
    std::string title;
    std::string artist;
    std::optional<int> durationSeconds;
};

struct MapDifficulty {
    Difficulty difficulty{Difficulty::Easy};
    std::string characteristic{"Standard"};
    double notesPerSecond{0.0};
    std::vector<std::string> requirements;
};

struct MapCandidate {
    std::string key;
    std::string hash;
    std::string songTitle;
    std::string songArtist;
    std::string mapper;
    std::optional<int> durationSeconds;
    double rating{0.0};
    std::uint32_t upvotes{0};
    std::uint32_t downvotes{0};
    bool curated{false};
    bool automapper{false};
    std::vector<MapDifficulty> difficulties;
    std::string coverUrl;
    std::string downloadUrl;
    bool stale{false};
};

struct MatchScores {
    double title{0.0};
    double artist{0.0};
    double duration{0.0};
    double identity{0.0};
    double quality{0.0};
    double suitability{0.0};
    double finalScore{0.0};
};

struct RecommendedMap {
    Track track;
    MapCandidate map;
    MatchScores scores;
    std::vector<MapDifficulty> playableDifficulties;
    bool installed{false};
};

struct RecommendationRequest {
    std::size_t maximumResults{20};
    std::size_t maximumTracks{60};
    std::set<std::string> excludedMapHashes;
    std::optional<std::string> currentTrackId;
};

} // namespace beatnext
