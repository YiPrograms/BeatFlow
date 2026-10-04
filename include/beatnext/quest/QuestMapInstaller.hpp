#pragma once

#include "beatnext/core/Interfaces.hpp"

#include <filesystem>

namespace beatnext::quest {

class QuestMapInstaller final : public MapInstaller {
  public:
    explicit QuestMapInstaller(HttpClient& http, std::filesystem::path stagingRoot);

    [[nodiscard]] bool isInstalled(const std::string& hash) const override;
    Outcome<std::string> install(const MapCandidate& map, const CancellationToken& cancellation) override;

  private:
    [[nodiscard]] static std::string safeFolderName(const MapCandidate& map);
    [[nodiscard]] static Outcome<bool> validateExtracted(const std::filesystem::path& root);

    HttpClient& http_;
    std::filesystem::path stagingRoot_;
};

} // namespace beatnext::quest
