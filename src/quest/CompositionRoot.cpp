#include "beatnext/quest/CompositionRoot.hpp"

#include "beatnext/quest/Logger.hpp"

#include "GlobalNamespace/BeatmapLevel.hpp"
#include "beatsaber-hook/shared/utils/il2cpp-functions.hpp"
#include "beatsaverplusplus/shared/BeatSaver.hpp"
#include "bsml/shared/BSML/MainThreadScheduler.hpp"
#include "songcore/shared/SongCore.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <fstream>
#include <future>

namespace beatnext::quest {
namespace {

constexpr auto kDataRoot = "/sdcard/ModData/com.beatgames.beatsaber/Mods/BeatNext";

std::string lowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return value;
}

std::optional<std::string> customHash(GlobalNamespace::BeatmapLevel* level) {
    if (level == nullptr)
        return std::nullopt;
    const std::string id = level->___levelID;
    constexpr std::string_view prefix = "custom_level_";
    if (!id.starts_with(prefix) || id.size() <= prefix.size())
        return std::nullopt;
    return lowerAscii(id.substr(prefix.size()));
}

std::function<void()> bindIl2Cpp(std::function<void()> task) {
    return [task = std::move(task)] {
        auto* thread = il2cpp_functions::thread_attach(il2cpp_functions::domain_get());
        try {
            task();
        } catch (...) {
            il2cpp_functions::thread_detach(thread);
            throw;
        }
        il2cpp_functions::thread_detach(thread);
    };
}

} // namespace

CompositionRoot& CompositionRoot::instance() {
    static CompositionRoot root;
    return root;
}

CompositionRoot::CompositionRoot()
    : dataRoot_(kDataRoot), http_(transport_), mapCache_(dataRoot_ / "cache" / "maps"),
      musicCache_(dataRoot_ / "cache" / "music"), music_(http_, &musicCache_),
      beatSaverCatalog_(http_, &mapCache_), catalog_(beatSaverCatalog_),
      mapInstaller_(http_, dataRoot_ / "staging"), engine_(music_, catalog_, &mapInstaller_), workers_(2, 32),
      sessionCancellation_(std::make_shared<CancellationSource>()) {}

CompositionRoot::~CompositionRoot() {
    shutdown();
}

void CompositionRoot::initialize() {
    {
        std::scoped_lock lock(stateMutex_);
        if (initialized_)
            return;
        initialized_ = true;
    }
    std::error_code ignored;
    std::filesystem::create_directories(dataRoot_, ignored);
    loadDisplaySettings();
    BeatSaver::API::Init(SongCore::API::Loading::GetPreferredCustomLevelPath());
}

void CompositionRoot::shutdown() {
    {
        std::scoped_lock lock(stateMutex_);
        if (!initialized_)
            return;
        sessionCancellation_->cancel();
        initialized_ = false;
    }
    workers_.stop();
}

void CompositionRoot::beginLevel(GlobalNamespace::BeatmapLevel* level) {
    if (level == nullptr)
        return;
    const CurrentSong song{std::string(level->___songName), std::string(level->___songAuthorName),
                           level->___songDuration > 0.0F
                               ? std::optional<int>(static_cast<int>(level->___songDuration + 0.5F))
                               : std::nullopt};

    std::shared_ptr<CancellationSource> cancellation;
    {
        std::scoped_lock lock(stateMutex_);
        if (const auto hash = customHash(level))
            playedHashes_.insert(*hash);
        sessionCancellation_->cancel();
        sessionCancellation_ = std::make_shared<CancellationSource>();
        cancellation = sessionCancellation_;
    }
    const auto generation = session_.begin("After " + song.title);
    if (song.title.empty() || song.artist.empty()) {
        session_.finish(generation, {},
                        ServiceError{ErrorCode::NotFound,
                                     "This level does not include enough song metadata for Up Next.", false,
                                     std::nullopt});
        return;
    }

    const bool queued = workers_.submit(bindIl2Cpp([this, cancellation, generation, song] {
        auto result = engine_.recommendAfter(song, request(), cancellation->token());
        dispatch([this, cancellation, generation, result = std::move(result)]() mutable {
            if (cancellation->isCancellationRequested())
                return;
            if (result) {
                const auto count = result.value().size();
                if (session_.finish(generation, std::move(result).value())) {
                    logger.info("Up Next prefetch found {} playable maps", count);
                }
            } else if (result.error().code != ErrorCode::Cancelled) {
                if (session_.finish(generation, {}, result.error())) {
                    logger.warn("Up Next prefetch failed: {}", result.error().message);
                }
            }
        });
    }));
    if (!queued) {
        session_.finish(generation, {},
                        ServiceError{ErrorCode::Internal, "BeatNext could not queue Up Next metadata work.",
                                     true, std::nullopt});
    }
}

RecommendationSessionState CompositionRoot::state() const {
    return session_.state();
}

std::uint64_t CompositionRoot::subscribe(StateCallback callback) {
    return session_.subscribe(std::move(callback));
}

void CompositionRoot::unsubscribe(std::uint64_t subscription) {
    session_.unsubscribe(subscription);
}

void CompositionRoot::select(std::size_t index) {
    static_cast<void>(session_.select(index));
}

void CompositionRoot::prepare(std::size_t index, PrepareCallback callback) {
    RecommendationItemState item;
    std::shared_ptr<CancellationSource> cancellation;
    std::uint64_t generation = 0;
    const auto snapshot = session_.state();
    if (index >= snapshot.items.size()) {
        dispatch([callback = std::move(callback)] {
            callback(Outcome<std::string>::failure(
                {ErrorCode::NotFound, "This recommendation is no longer available.", false, std::nullopt}));
        });
        return;
    }
    item = snapshot.items[index];
    generation = snapshot.generation;
    {
        std::scoped_lock lock(stateMutex_);
        cancellation = sessionCancellation_;
    }
    session_.updateItem(generation, index, RecommendationItemStatus::Downloading,
                        item.recommendation.installed ? "Preparing song…" : "Downloading…");

    if (!workers_.submit(bindIl2Cpp([this, cancellation, generation, index, item = std::move(item),
                                     callback = std::move(callback)]() mutable {
            Outcome<std::string> result = Outcome<std::string>::success(item.recommendation.map.hash);
            if (!mapInstaller_.isInstalled(item.recommendation.map.hash)) {
                result = mapInstaller_.install(item.recommendation.map, cancellation->token());
                if (result) {
                    const auto refresh = SongCore::API::Loading::RefreshSongs(false);
                    if (refresh.wait_for(std::chrono::seconds(30)) != std::future_status::ready) {
                        result = Outcome<std::string>::failure(
                            {ErrorCode::Internal,
                             "SongCore did not finish refreshing custom songs within 30 seconds.", true,
                             std::nullopt});
                    } else if (!mapInstaller_.isInstalled(result.value())) {
                        result = Outcome<std::string>::failure(
                            {ErrorCode::NotFound, "SongCore refreshed but could not load the downloaded map.",
                             true, std::nullopt});
                    }
                }
            }
            dispatch([this, cancellation, generation, index, callback = std::move(callback),
                      result = std::move(result)]() mutable {
                if (cancellation->isCancellationRequested())
                    return;
                const bool current =
                    result ? session_.updateItem(generation, index, RecommendationItemStatus::Installed,
                                                 "Installed", true)
                           : session_.updateItem(generation, index, RecommendationItemStatus::Failed,
                                                 result.error().message);
                if (!current)
                    return;
                callback(std::move(result));
            });
        }))) {
        session_.updateItem(generation, index, RecommendationItemStatus::Failed,
                            "BeatNext's worker queue is full.");
        callback(Outcome<std::string>::failure(
            {ErrorCode::Internal, "BeatNext's worker queue is full. Try again.", true, std::nullopt}));
    }
}

void CompositionRoot::rememberPlayed(const std::string& hash) {
    std::scoped_lock lock(stateMutex_);
    playedHashes_.insert(lowerAscii(hash));
}

bool CompositionRoot::showNextOnResults() const {
    std::scoped_lock lock(stateMutex_);
    return showNextOnResults_;
}
bool CompositionRoot::showNextOnPause() const {
    std::scoped_lock lock(stateMutex_);
    return showNextOnPause_;
}
void CompositionRoot::setShowNextOnResults(bool value) {
    {
        std::scoped_lock lock(stateMutex_);
        showNextOnResults_ = value;
    }
    saveDisplaySettings();
}
void CompositionRoot::setShowNextOnPause(bool value) {
    {
        std::scoped_lock lock(stateMutex_);
        showNextOnPause_ = value;
    }
    saveDisplaySettings();
}

void CompositionRoot::loadDisplaySettings() {
    std::ifstream input(dataRoot_ / "settings.json");
    if (!input)
        return;
    try {
        const auto value = nlohmann::json::parse(input);
        std::scoped_lock lock(stateMutex_);
        showNextOnResults_ = value.value("showUpNextOnScore", true);
        showNextOnPause_ = value.value("showUpNextOnPause", true);
    } catch (...) {
    }
}

void CompositionRoot::saveDisplaySettings() const {
    bool results = true;
    bool pause = true;
    {
        std::scoped_lock lock(stateMutex_);
        results = showNextOnResults_;
        pause = showNextOnPause_;
    }
    const auto path = dataRoot_ / "settings.json";
    const auto temporary = dataRoot_ / "settings.json.tmp";
    std::ofstream output(temporary, std::ios::trunc);
    if (!output)
        return;
    output << nlohmann::json{{"showUpNextOnScore", results}, {"showUpNextOnPause", pause}}.dump(2);
    output.close();
    std::error_code error;
    std::filesystem::rename(temporary, path, error);
    if (error) {
        std::filesystem::remove(path, error);
        error.clear();
        std::filesystem::rename(temporary, path, error);
    }
}

RecommendationRequest CompositionRoot::request() const {
    std::scoped_lock lock(stateMutex_);
    RecommendationRequest result;
    result.maximumResults = 20;
    result.maximumTracks = 60;
    result.excludedMapHashes = playedHashes_;
    return result;
}

void CompositionRoot::dispatch(std::function<void()> callback) {
    BSML::MainThreadScheduler::Schedule(std::move(callback));
}

} // namespace beatnext::quest
