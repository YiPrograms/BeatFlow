#include "beatflow/quest/SongDetailsCatalog.hpp"

#include "beatflow/core/TextNormalizer.hpp"

#include "song-details/shared/SongDetails.hpp"

#include <algorithm>
#include <cctype>
#include <set>
#include <string_view>
#include <unordered_set>

namespace beatflow::quest {
namespace {

std::string lowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return value;
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

Difficulty difficulty(SongDetailsCache::MapDifficulty value) {
    switch (value) {
    case SongDetailsCache::MapDifficulty::Easy:
        return Difficulty::Easy;
    case SongDetailsCache::MapDifficulty::Normal:
        return Difficulty::Normal;
    case SongDetailsCache::MapDifficulty::Hard:
        return Difficulty::Hard;
    case SongDetailsCache::MapDifficulty::Expert:
        return Difficulty::Expert;
    case SongDetailsCache::MapDifficulty::ExpertPlus:
        return Difficulty::ExpertPlus;
    }
    return Difficulty::ExpertPlus;
}

std::string characteristic(SongDetailsCache::MapCharacteristic value) {
    switch (value) {
    case SongDetailsCache::MapCharacteristic::Standard:
        return "Standard";
    case SongDetailsCache::MapCharacteristic::OneSaber:
        return "OneSaber";
    case SongDetailsCache::MapCharacteristic::NoArrows:
        return "NoArrows";
    case SongDetailsCache::MapCharacteristic::NinetyDegree:
        return "90Degree";
    case SongDetailsCache::MapCharacteristic::ThreeSixtyDegree:
        return "360Degree";
    case SongDetailsCache::MapCharacteristic::LightShow:
        return "Lightshow";
    case SongDetailsCache::MapCharacteristic::Lawless:
        return "Lawless";
    case SongDetailsCache::MapCharacteristic::Custom:
        return "Custom";
    }
    return "Custom";
}

MapCandidate convert(const SongDetailsCache::Song& song) {
    MapCandidate map;
    map.key = song.key();
    map.hash = song.hash();
    map.songTitle = song.songName();
    map.songArtist = song.songAuthorName();
    map.mapper = song.levelAuthorName();
    if (song.songDurationSeconds > 0) {
        map.durationSeconds = static_cast<int>(song.songDurationSeconds);
    }
    map.rating = song.rating();
    map.upvotes = song.upvotes;
    map.downvotes = song.downvotes;
    map.curated = SongDetailsCache::hasFlags(song.uploadFlags, SongDetailsCache::UploadFlags::Curated);
    map.coverUrl = song.coverURL();
    map.downloadUrl = "https://r2cdn.beatsaver.com/" + lowerAscii(map.hash) + ".zip";
    for (const auto& source : song) {
        const auto nps = song.songDurationSeconds == 0 ? 0.0
                                                       : static_cast<double>(source.notes) /
                                                             static_cast<double>(song.songDurationSeconds);
        map.difficulties.push_back({difficulty(source.difficulty), characteristic(source.characteristic), nps,
                                    SongDetailsCache::toVectorOfStrings(source.mods)});
    }
    return map;
}

} // namespace

SongDetailsCatalog::SongDetailsCatalog(BeatSaverCatalog& fallback) : fallback_(fallback) {}

bool SongDetailsCatalog::ensureIndex() {
    std::scoped_lock lock(mutex_);
    if (initialized_) {
        return details_ != nullptr && details_->songs.get_isDataAvailable();
    }
    initialized_ = true;
    try {
        details_ = SongDetailsCache::SongDetails::Init().get();
    } catch (...) {
        details_ = nullptr;
    }
    if (details_ == nullptr || !details_->songs.get_isDataAvailable()) {
        return false;
    }

    TextNormalizer normalizer;
    for (const auto& song : details_->songs) {
        std::set<std::string> uniqueTokens;
        for (const auto& token : normalizer.title(song.songName()).tokens) {
            if (token.size() >= 2) {
                uniqueTokens.insert(token);
            }
        }
        for (const auto& token : uniqueTokens) {
            titleTokenIndex_[token].push_back(song.mapId());
        }
    }
    return true;
}

Outcome<std::vector<MapCandidate>> SongDetailsCatalog::search(const Track& track,
                                                              const CancellationToken& cancellation) {
    if (cancellation.isCancellationRequested()) {
        return Outcome<std::vector<MapCandidate>>::failure(
            {ErrorCode::Cancelled, "The map catalog search was cancelled.", false, std::nullopt});
    }
    if (!ensureIndex()) {
        return fallback_.search(track, cancellation);
    }

    TextNormalizer normalizer;
    std::unordered_set<std::uint32_t> mapIds;
    for (const auto alias : titleAliases(track.title)) {
        const auto normalized = normalizer.title(alias);
        const std::vector<std::uint32_t>* rarest = nullptr;
        for (const auto& token : normalized.tokens) {
            const auto found = titleTokenIndex_.find(token);
            if (found != titleTokenIndex_.end() &&
                (rarest == nullptr || found->second.size() < rarest->size())) {
                rarest = &found->second;
            }
        }
        if (rarest != nullptr) {
            mapIds.insert(rarest->begin(), rarest->end());
        }
    }

    std::vector<MapCandidate> maps;
    maps.reserve(std::min<std::size_t>(mapIds.size(), 256));
    for (const auto id : mapIds) {
        if (cancellation.isCancellationRequested()) {
            return Outcome<std::vector<MapCandidate>>::failure(
                {ErrorCode::Cancelled, "The map catalog search was cancelled.", false, std::nullopt});
        }
        const auto& song = details_->songs.FindByMapId(id);
        if (song && maps.size() < 256) {
            maps.push_back(convert(song));
        }
    }
    if (!maps.empty()) {
        return Outcome<std::vector<MapCandidate>>::success(std::move(maps));
    }
    return fallback_.search(track, cancellation);
}

} // namespace beatflow::quest
