#include "beatflow/quest/CompositionRoot.hpp"

#include "beatflow/quest/BuildConfig.hpp"
#include "beatflow/quest/Logger.hpp"

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
#include <thread>

namespace beatflow::quest {
namespace {

constexpr auto kDataRoot = "/sdcard/ModData/com.beatgames.beatsaber/Mods/BeatFlow";

std::string lowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return value;
}

std::optional<std::string> customHash(GlobalNamespace::BeatmapLevel* level) {
    if (level == nullptr) {
        return std::nullopt;
    }
    const std::string id = level->___levelID;
    constexpr std::string_view prefix = "custom_level_";
    if (!id.starts_with(prefix) || id.size() <= prefix.size()) {
        return std::nullopt;
    }
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
      musicCache_(dataRoot_ / "cache" / "music"), personalizedCache_(dataRoot_ / "cache" / "accounts"),
      credentials_(dataRoot_, {std::string(build::kOAuthClientId), std::string(build::kOAuthClientSecret)}),
      oauth_(http_, credentials_), music_(http_, oauth_, &musicCache_, &personalizedCache_),
      beatSaverCatalog_(http_, &mapCache_), catalog_(beatSaverCatalog_),
      library_(http_, dataRoot_ / "staging"), engine_(music_, catalog_, &library_), workers_(2, 32),
      interactiveCancellation_(std::make_shared<CancellationSource>()),
      prefetchCancellation_(std::make_shared<CancellationSource>()) {}

CompositionRoot::~CompositionRoot() {
    shutdown();
}

void CompositionRoot::initialize() {
    {
        std::scoped_lock lock(stateMutex_);
        if (initialized_) {
            return;
        }
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
        if (!initialized_) {
            return;
        }
        interactiveCancellation_->cancel();
        prefetchCancellation_->cancel();
        initialized_ = false;
    }
    workers_.stop();
}

void CompositionRoot::refreshForYou(RecommendationFilters filters, RecommendationCallback callback) {
    std::shared_ptr<CancellationSource> cancellation;
    std::uint64_t generation = 0;
    {
        std::scoped_lock lock(stateMutex_);
        interactiveCancellation_->cancel();
        interactiveCancellation_ = std::make_shared<CancellationSource>();
        cancellation = interactiveCancellation_;
        generation = ++interactiveGeneration_;
        filters_ = std::move(filters);
        forYouState_ = {{}, std::nullopt, "Personalized For You", true, false};
        browseState_ = forYouState_;
        browseMode_ = BrowseMode::ForYou;
    }
    dispatch([callback] { callback({{}, std::nullopt, "Personalized For You", true, false}); });

    auto workerCallback = callback;
    if (!workers_.submit(bindIl2Cpp([this, cancellation, generation, callback = std::move(workerCallback)] {
            auto operationRequest = request(20);
            std::vector<RecommendedMap> progressive;
            auto result = engine_.forYou(
                operationRequest, cancellation->token(),
                [this, cancellation, generation, callback, &progressive](const RecommendedMap& match) {
                    progressive.push_back(match);
                    RecommendationViewState snapshot{progressive, std::nullopt, "Personalized For You", true,
                                                     std::ranges::any_of(progressive, [](const auto& item) {
                                                         return item.track.stale || item.map.stale;
                                                     })};
                    dispatch([this, cancellation, generation, callback, snapshot = std::move(snapshot)] {
                        {
                            std::scoped_lock lock(stateMutex_);
                            if (cancellation != interactiveCancellation_ ||
                                generation != interactiveGeneration_) {
                                return;
                            }
                            forYouState_ = snapshot;
                            if (browseMode_ == BrowseMode::ForYou) {
                                browseState_ = snapshot;
                            }
                        }
                        callback(snapshot);
                    });
                });

            RecommendationViewState finished;
            finished.context = "Personalized For You";
            if (result) {
                finished.recommendations = std::move(result).value();
                finished.stale = std::ranges::any_of(finished.recommendations, [](const auto& item) {
                    return item.track.stale || item.map.stale;
                });
            } else if (result.error().code != ErrorCode::Cancelled) {
                finished.error = result.error();
            }
            dispatch([this, cancellation, generation, callback, finished = std::move(finished)] {
                {
                    std::scoped_lock lock(stateMutex_);
                    if (cancellation != interactiveCancellation_ || generation != interactiveGeneration_) {
                        return;
                    }
                    forYouState_ = finished;
                    if (browseMode_ == BrowseMode::ForYou) {
                        browseState_ = finished;
                    }
                }
                callback(finished);
            });
        }))) {
        RecommendationViewState failed{{},
                                       ServiceError{ErrorCode::Internal,
                                                    "BeatFlow's worker queue is full. Try again.", true,
                                                    std::nullopt},
                                       "Personalized For You",
                                       false,
                                       false};
        dispatch([callback = std::move(callback), failed] { callback(failed); });
    }
}

void CompositionRoot::connect(AuthorizationCallback callback) {
    std::shared_ptr<CancellationSource> cancellation;
    std::uint64_t generation = 0;
    {
        std::scoped_lock lock(stateMutex_);
        interactiveCancellation_->cancel();
        interactiveCancellation_ = std::make_shared<CancellationSource>();
        cancellation = interactiveCancellation_;
        generation = ++interactiveGeneration_;
    }
    dispatch([callback] { callback({"Starting Google device authorization…", "", "", false, true}); });

    auto unavailableCallback = callback;
    if (!workers_.submit(bindIl2Cpp([this, cancellation, generation, callback = std::move(callback)] {
            const auto publish = [this, cancellation, generation, callback](AuthorizationViewState state) {
                dispatch([this, cancellation, generation, callback, state = std::move(state)] {
                    {
                        std::scoped_lock lock(stateMutex_);
                        if (cancellation != interactiveCancellation_ ||
                            generation != interactiveGeneration_) {
                            return;
                        }
                    }
                    callback(state);
                });
            };
            auto authorization = oauth_.begin(cancellation->token());
            if (!authorization) {
                publish({authorization.error().message, "", "", false, false});
                return;
            }
            const auto device = authorization.value();
            publish({"The Google sign-in page is opening. Enter this code in the browser.",
                     device.verificationUrl, device.userCode, false, true});

            auto nextDelay = device.pollingIntervalSeconds;
            const auto expiresAt =
                std::chrono::steady_clock::now() + std::chrono::seconds(device.expiresInSeconds);
            while (!cancellation->isCancellationRequested() && std::chrono::steady_clock::now() < expiresAt) {
                for (int elapsed = 0; elapsed < nextDelay * 10 && !cancellation->isCancellationRequested();
                     ++elapsed) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
                if (cancellation->isCancellationRequested()) {
                    return;
                }
                auto poll = oauth_.poll(device, cancellation->token());
                if (!poll) {
                    publish({poll.error().message, "", "", false, false});
                    return;
                }
                nextDelay = poll.value().nextPollSeconds;
                if (poll.value().status == AuthorizationStatus::Pending ||
                    poll.value().status == AuthorizationStatus::SlowDown) {
                    continue;
                }
                if (poll.value().status != AuthorizationStatus::Complete) {
                    publish({"Google authorization expired or was denied. Try Connect again.", "", "", false,
                             false});
                    return;
                }

                auto home = music_.home(cancellation->token());
                if (!home || home.value().empty()) {
                    const auto message =
                        home ? "No recommendations were found from this account's liked videos."
                             : home.error().message;
                    publish({message, "", "", false, false});
                    return;
                }
                auto radio = music_.radio(home.value().front().providerId, cancellation->token());
                if (!radio) {
                    publish({"The account connected, but validation failed: " + radio.error().message, "", "",
                             false, false});
                    return;
                }
                publish({"YouTube connected. For You is ready from your liked videos.", "", "", true, false});
                return;
            }
            publish({"The Google device code expired. Select Connect to try again.", "", "", false, false});
        }))) {
        dispatch([callback = std::move(unavailableCallback)] {
            callback({"BeatFlow's worker queue is full. Try Connect again.", "", "", false, false});
        });
    }
}

void CompositionRoot::cancelInteractive() {
    std::scoped_lock lock(stateMutex_);
    interactiveCancellation_->cancel();
    ++interactiveGeneration_;
}

Outcome<bool> CompositionRoot::disconnect() {
    cancelInteractive();
    auto cleared = credentials_.clearAll();
    if (!cleared) {
        return cleared;
    }
    auto cache = personalizedCache_.clear();
    if (cache) {
        std::scoped_lock lock(stateMutex_);
        forYouState_ = {};
        if (browseMode_ == BrowseMode::ForYou) {
            browseState_ = {};
        }
    }
    return cache;
}

Outcome<bool> CompositionRoot::clearLocalData() {
    cancelInteractive();
    auto account = credentials_.clearAll();
    if (!account) {
        return account;
    }
    auto maps = mapCache_.clear();
    if (!maps) {
        return maps;
    }
    auto music = musicCache_.clear();
    if (!music) {
        return music;
    }
    auto personalized = personalizedCache_.clear();
    if (!personalized) {
        return personalized;
    }
    std::error_code ignored;
    std::filesystem::remove_all(dataRoot_ / "staging", ignored);
    return Outcome<bool>::success(true);
}

void CompositionRoot::prefetchForLevel(GlobalNamespace::BeatmapLevel* level) {
    if (level == nullptr) {
        return;
    }
    const std::string title = level->___songName;
    const std::string artist = level->___songAuthorName;
    const auto duration = level->___songDuration > 0.0F
                              ? std::optional<int>(static_cast<int>(level->___songDuration + 0.5F))
                              : std::nullopt;
    if (title.empty() || artist.empty()) {
        std::scoped_lock lock(stateMutex_);
        nextState_ = {{},
                      ServiceError{ErrorCode::NotFound,
                                   "This level does not include enough song metadata for Up Next.", false,
                                   std::nullopt},
                      title,
                      false,
                      false};
        return;
    }

    std::shared_ptr<CancellationSource> cancellation;
    std::uint64_t generation = 0;
    {
        std::scoped_lock lock(stateMutex_);
        if (const auto hash = customHash(level)) {
            playedHashes_.insert(*hash);
        }
        prefetchCancellation_->cancel();
        prefetchCancellation_ = std::make_shared<CancellationSource>();
        cancellation = prefetchCancellation_;
        generation = ++prefetchGeneration_;
        nextState_ = {{}, std::nullopt, "After " + title, true, false};
        if (browseMode_ == BrowseMode::Next) {
            browseState_ = nextState_;
        }
    }

    const bool queued = workers_.submit(bindIl2Cpp([this, cancellation, generation, title, artist, duration] {
        auto result = engine_.upNext(title, artist, duration, request(20), cancellation->token());
        RecommendationViewState state;
        state.context = "After " + title;
        if (result) {
            state.recommendations = std::move(result).value();
            state.stale = std::ranges::any_of(
                state.recommendations, [](const auto& item) { return item.track.stale || item.map.stale; });
        } else if (result.error().code != ErrorCode::Cancelled) {
            state.error = result.error();
        }
        std::scoped_lock lock(stateMutex_);
        if (cancellation == prefetchCancellation_ && generation == prefetchGeneration_) {
            if (state.error) {
                logger.warn("Up Next prefetch failed: {}", state.error->message);
            } else {
                logger.info("Up Next prefetch found {} playable maps", state.recommendations.size());
            }
            nextState_ = state;
            if (browseMode_ == BrowseMode::Next) {
                browseState_ = std::move(state);
            }
        }
    }));
    if (!queued) {
        std::scoped_lock lock(stateMutex_);
        nextState_ = {{},
                      ServiceError{ErrorCode::Internal, "BeatFlow could not queue Up Next metadata work.",
                                   true, std::nullopt},
                      "After " + title,
                      false,
                      false};
    }
}

RecommendationViewState CompositionRoot::nextState() const {
    std::scoped_lock lock(stateMutex_);
    return nextState_;
}

RecommendationViewState CompositionRoot::browseState() const {
    std::scoped_lock lock(stateMutex_);
    return browseState_;
}

void CompositionRoot::browseForYouRecommendations() {
    std::scoped_lock lock(stateMutex_);
    browseMode_ = BrowseMode::ForYou;
    browseState_ = forYouState_;
}

void CompositionRoot::browseNextRecommendations() {
    std::scoped_lock lock(stateMutex_);
    browseMode_ = BrowseMode::Next;
    browseState_ = nextState_;
}

void CompositionRoot::prepare(const RecommendedMap& recommendation, PrepareCallback callback) {
    auto cancellation = std::make_shared<CancellationSource>();
    auto workerCallback = callback;
    if (!workers_.submit(
            bindIl2Cpp([this, cancellation, recommendation, callback = std::move(workerCallback)] {
                if (library_.isInstalled(recommendation.map.hash)) {
                    dispatch([callback, hash = recommendation.map.hash] {
                        callback(Outcome<std::string>::success(hash));
                    });
                    return;
                }
                auto installed = library_.install(recommendation.map, cancellation->token());
                if (installed) {
                    SongCore::API::Loading::RefreshSongs(false).wait();
                } else {
                    logger.warn("Map installation failed: {}", installed.error().message);
                }
                dispatch([callback, installed = std::move(installed)]() mutable {
                    callback(std::move(installed));
                });
            }))) {
        dispatch([callback = std::move(callback)] {
            callback(Outcome<std::string>::failure(
                {ErrorCode::Internal, "BeatFlow's worker queue is full. Try again.", true, std::nullopt}));
        });
    }
}

Outcome<bool> CompositionRoot::openPrepared(const std::string& hash) {
    rememberPlayed(hash);
    return library_.openSongDetails(hash);
}

void CompositionRoot::rememberPlayed(const std::string& hash) {
    std::scoped_lock lock(stateMutex_);
    playedHashes_.insert(lowerAscii(hash));
}

RecommendationFilters CompositionRoot::filters() const {
    std::scoped_lock lock(stateMutex_);
    return filters_;
}

void CompositionRoot::setFilters(RecommendationFilters filters) {
    std::scoped_lock lock(stateMutex_);
    filters_ = std::move(filters);
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
    const auto path = dataRoot_ / "settings.json";
    std::ifstream input(path);
    if (!input) {
        return;
    }
    try {
        const auto value = nlohmann::json::parse(input);
        std::scoped_lock lock(stateMutex_);
        showNextOnResults_ = value.value("showUpNextOnSongEnd", true);
        showNextOnPause_ = value.value("showUpNextOnPause", true);
    } catch (...) {
        // Invalid settings are ignored so the safe defaults remain available.
    }
}

void CompositionRoot::saveDisplaySettings() const {
    bool onResults = true;
    bool onPause = true;
    {
        std::scoped_lock lock(stateMutex_);
        onResults = showNextOnResults_;
        onPause = showNextOnPause_;
    }
    const auto path = dataRoot_ / "settings.json";
    const auto temporary = dataRoot_ / "settings.json.tmp";
    std::ofstream output(temporary, std::ios::trunc);
    if (!output) {
        return;
    }
    output << nlohmann::json{{"showUpNextOnSongEnd", onResults}, {"showUpNextOnPause", onPause}}.dump(2);
    output.close();
    if (!output) {
        return;
    }
    std::error_code error;
    std::filesystem::rename(temporary, path, error);
    if (error) {
        std::filesystem::remove(path, error);
        error.clear();
        std::filesystem::rename(temporary, path, error);
    }
}

RecommendationRequest CompositionRoot::request(std::size_t maximumResults) const {
    std::scoped_lock lock(stateMutex_);
    RecommendationRequest result;
    result.filters = filters_;
    result.maximumResults = maximumResults;
    result.maximumTracks = 60;
    result.excludedMapHashes = playedHashes_;
    return result;
}

void CompositionRoot::dispatch(std::function<void()> callback) {
    BSML::MainThreadScheduler::Schedule(std::move(callback));
}

} // namespace beatflow::quest
