#include "beatnext/quest/QuestMapInstaller.hpp"

#include "beatnext/services/ZipArchiveExtractor.hpp"

#include "songcore/shared/SongCore.hpp"

#include <algorithm>
#include <span>

namespace beatnext::quest {
namespace {

ServiceError storageError(std::string message) {
    return {ErrorCode::Storage, std::move(message), false, std::nullopt};
}

} // namespace

QuestMapInstaller::QuestMapInstaller(HttpClient& http, std::filesystem::path stagingRoot)
    : http_(http), stagingRoot_(std::move(stagingRoot)) {}

bool QuestMapInstaller::isInstalled(const std::string& hash) const {
    return SongCore::API::Loading::GetLevelByHash(hash) != nullptr;
}

Outcome<std::string> QuestMapInstaller::install(const MapCandidate& map,
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
    request.headers = {{"Accept", "application/zip"}, {"User-Agent", "BeatNext/0.1"}};
    request.timeoutSeconds = 90;
    request.maximumResponseBytes = 256U * 1024U * 1024U;
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
    const auto extracted = operationRoot / "extracted";
    auto extraction = ZipArchiveExtractor::extract(archive, extracted);
    if (!extraction) {
        std::filesystem::remove_all(operationRoot, error);
        return Outcome<std::string>::failure(extraction.error());
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

std::string QuestMapInstaller::safeFolderName(const MapCandidate& map) {
    std::string name = "BeatNext_" + map.key + "_" + map.songTitle;
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

Outcome<bool> QuestMapInstaller::validateExtracted(const std::filesystem::path& root) {
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

} // namespace beatnext::quest
