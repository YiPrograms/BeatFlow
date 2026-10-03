#pragma once

#include "beatflow/core/Interfaces.hpp"

#include <filesystem>

namespace beatflow::quest {

class QuestSongLibrary final : public SongLibrary {
  public:
    explicit QuestSongLibrary(HttpClient& http, std::filesystem::path stagingRoot);

    [[nodiscard]] bool isInstalled(const std::string& hash) const override;
    Outcome<std::string> install(const MapCandidate& map, const CancellationToken& cancellation) override;
    Outcome<bool> openSongDetails(const std::string& hash) override;

  private:
    [[nodiscard]] static std::string safeFolderName(const MapCandidate& map);
    [[nodiscard]] static Outcome<bool> validateExtracted(const std::filesystem::path& root);

    HttpClient& http_;
    std::filesystem::path stagingRoot_;
};

} // namespace beatflow::quest
