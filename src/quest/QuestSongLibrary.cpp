#include "beatflow/quest/QuestSongLibrary.hpp"

#include "beatflow/quest/Logger.hpp"
#include "beatflow/services/ZipArchiveValidator.hpp"

#include "GlobalNamespace/BeatmapLevel.hpp"
#include "GlobalNamespace/BeatmapLevelPack.hpp"
#include "GlobalNamespace/LevelSelectionFlowCoordinator.hpp"
#include "GlobalNamespace/SelectLevelCategoryViewController.hpp"
#include "GlobalNamespace/SoloFreePlayFlowCoordinator.hpp"
#include "HMUI/NoTransitionsButton.hpp"
#include "System/Nullable_1.hpp"
#include "UnityEngine/GameObject.hpp"
#include "beatsaverplusplus/shared/BeatSaver.hpp"
#include "bsml/shared/Helpers/getters.hpp"
#include "songcore/shared/SongCore.hpp"

#include <algorithm>
#include <fstream>
#include <span>

namespace beatflow::quest {
namespace {

ServiceError storageError(std::string message) {
    return {ErrorCode::Storage, std::move(message), false, std::nullopt};
}

} // namespace

QuestSongLibrary::QuestSongLibrary(HttpClient& http, std::filesystem::path stagingRoot)
    : http_(http), stagingRoot_(std::move(stagingRoot)) {}

bool QuestSongLibrary::isInstalled(const std::string& hash) const {
    return SongCore::API::Loading::GetLevelByHash(hash) != nullptr;
}

Outcome<std::string> QuestSongLibrary::install(const MapCandidate& map,
                                               const CancellationToken& cancellation) {
    if (isInstalled(map.hash)) {
        return Outcome<std::string>::success(map.hash);
    }
    if (map.downloadUrl.empty()) {
        return Outcome<std::string>::failure(
            {ErrorCode::InvalidResponse, "BeatSaver did not provide a download URL.", false, std::nullopt});
    }

    HttpRequest request;
    request.url = map.downloadUrl;
    request.headers = {{"Accept", "application/zip"}, {"User-Agent", "BeatFlow/0.1"}};
    request.timeoutSeconds = 90;
    auto response = http_.send(request, cancellation);
    if (!response) {
        return Outcome<std::string>::failure(response.error());
    }
    if (response.value().status < 200 || response.value().status >= 300) {
        return Outcome<std::string>::failure(
            {ErrorCode::Network,
             "BeatSaver returned HTTP " + std::to_string(response.value().status) + " for the map archive.",
             response.value().status == 429 || response.value().status >= 500, std::nullopt});
    }
    const auto* archiveData = reinterpret_cast<const std::uint8_t*>(response.value().body.data());
    const std::span<const std::uint8_t> archive(archiveData, response.value().body.size());
    auto valid = ZipArchiveValidator::validate(archive);
    if (!valid) {
        return Outcome<std::string>::failure(valid.error());
    }
    if (cancellation.isCancellationRequested()) {
        return Outcome<std::string>::failure(
            {ErrorCode::Cancelled, "The map installation was cancelled.", false, std::nullopt});
    }

    std::error_code error;
    const auto operationRoot = stagingRoot_ / map.hash;
    std::filesystem::remove_all(operationRoot, error);
    error.clear();
    std::filesystem::create_directories(operationRoot, error);
    if (error) {
        return Outcome<std::string>::failure(
            storageError("Could not create the map staging directory: " + error.message()));
    }
    const auto archivePath = operationRoot / "map.zip";
    {
        std::ofstream output(archivePath, std::ios::binary | std::ios::trunc);
        output.write(response.value().body.data(),
                     static_cast<std::streamsize>(response.value().body.size()));
        if (!output) {
            std::filesystem::remove_all(operationRoot, error);
            return Outcome<std::string>::failure(storageError("Could not stage the downloaded map archive."));
        }
    }

    const auto extracted = operationRoot / "extracted";
    std::filesystem::create_directories(extracted, error);
    const auto fileUrl = "file://" + archivePath.string();
    if (!BeatSaver::API::DownloadSongZip(WebUtils::URLOptions(fileUrl), extracted)) {
        std::filesystem::remove_all(operationRoot, error);
        return Outcome<std::string>::failure({ErrorCode::InvalidResponse,
                                              "The validated BeatSaver archive could not be extracted.",
                                              false, std::nullopt});
    }
    auto extractedValid = validateExtracted(extracted);
    if (!extractedValid) {
        std::filesystem::remove_all(operationRoot, error);
        return Outcome<std::string>::failure(extractedValid.error());
    }

    auto sourceRoot = extracted;
    if (!std::filesystem::exists(sourceRoot / "Info.dat") &&
        !std::filesystem::exists(sourceRoot / "info.dat")) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(extracted, error)) {
            if (error) {
                break;
            }
            auto name = entry.path().filename().string();
            std::transform(name.begin(), name.end(), name.begin(), [](unsigned char character) {
                return static_cast<char>(std::tolower(character));
            });
            if (entry.is_regular_file() && name == "info.dat") {
                sourceRoot = entry.path().parent_path();
                break;
            }
        }
    }

    const auto destination = SongCore::API::Loading::GetPreferredCustomLevelPath() / safeFolderName(map);
    std::filesystem::remove_all(destination, error);
    error.clear();
    std::filesystem::rename(sourceRoot, destination, error);
    if (error) {
        std::filesystem::remove_all(operationRoot, error);
        return Outcome<std::string>::failure(
            storageError("Could not publish the staged map: " + error.message()));
    }
    std::filesystem::remove_all(operationRoot, error);
    return Outcome<std::string>::success(map.hash);
}

Outcome<bool> QuestSongLibrary::openSongDetails(const std::string& hash) {
    auto* level = SongCore::API::Loading::GetLevelByHash(hash);
    if (level == nullptr) {
        return Outcome<bool>::failure({ErrorCode::NotFound,
                                       "The installed map has not been loaded by SongCore yet.", true,
                                       std::nullopt});
    }
    auto* pack = SongCore::API::Loading::GetCustomLevelPack();
    auto* solo = BSML::Helpers::GetDiContainer()->Resolve<GlobalNamespace::SoloFreePlayFlowCoordinator*>();
    if (pack == nullptr || solo == nullptr) {
        return Outcome<bool>::failure(
            {ErrorCode::Internal, "Beat Saber's solo song picker is unavailable.", true, std::nullopt});
    }

    auto category = GlobalNamespace::SelectLevelCategoryViewController::LevelCategory::All;
    System::Nullable_1<GlobalNamespace::SelectLevelCategoryViewController::LevelCategory> nullableCategory;
    nullableCategory.value = category;
    nullableCategory.hasValue = true;
    auto* state = GlobalNamespace::LevelSelectionFlowCoordinator::State::New_ctor(
        static_cast<GlobalNamespace::BeatmapLevelPack*>(pack),
        static_cast<GlobalNamespace::BeatmapLevel*>(level));
    state->___levelCategory = nullableCategory;
    solo->Setup(state);

    auto buttonObject = UnityEngine::GameObject::Find("SoloButton");
    if (buttonObject == nullptr) {
        return Outcome<bool>::failure(
            {ErrorCode::Internal, "Beat Saber's Solo button could not be found.", true, std::nullopt});
    }
    auto* button = buttonObject->GetComponent<HMUI::NoTransitionsButton*>();
    if (button == nullptr) {
        return Outcome<bool>::failure(
            {ErrorCode::Internal, "Beat Saber's Solo button is unavailable.", true, std::nullopt});
    }
    button->Press();
    return Outcome<bool>::success(true);
}

std::string QuestSongLibrary::safeFolderName(const MapCandidate& map) {
    std::string name = "BeatFlow_" + map.key + "_" + map.songTitle;
    for (auto& character : name) {
        const auto byte = static_cast<unsigned char>(character);
        if (byte < 0x20U || character == '/' || character == '\\' || character == ':' || character == '*' ||
            character == '?' || character == '"' || character == '<' || character == '>' ||
            character == '|') {
            character = '_';
        }
    }
    if (name.size() > 96) {
        name.resize(96);
    }
    return name;
}

Outcome<bool> QuestSongLibrary::validateExtracted(const std::filesystem::path& root) {
    std::error_code error;
    const auto canonicalRoot = std::filesystem::weakly_canonical(root, error);
    if (error) {
        return Outcome<bool>::failure(storageError("Could not inspect the staged map."));
    }
    bool infoFound = false;
    std::size_t files = 0;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root, error)) {
        if (error || ++files > 4096) {
            return Outcome<bool>::failure(storageError("The staged map contains too many files."));
        }
        if (entry.is_symlink(error)) {
            return Outcome<bool>::failure(storageError("The staged map contains an unsupported link."));
        }
        const auto canonical = std::filesystem::weakly_canonical(entry.path(), error);
        const auto relative = canonical.lexically_relative(canonicalRoot);
        if (error || relative.empty() || relative.native().starts_with("..")) {
            return Outcome<bool>::failure(storageError("The staged map escaped its installation directory."));
        }
        auto name = entry.path().filename().string();
        std::transform(name.begin(), name.end(), name.begin(),
                       [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
        infoFound = infoFound || (entry.is_regular_file() && name == "info.dat");
    }
    if (!infoFound) {
        return Outcome<bool>::failure(storageError("The staged map does not contain Info.dat."));
    }
    return Outcome<bool>::success(true);
}

} // namespace beatflow::quest
