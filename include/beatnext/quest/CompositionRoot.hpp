#pragma once

#include "beatnext/core/RecommendationEngine.hpp"
#include "beatnext/core/RecommendationSession.hpp"
#include "beatnext/quest/QuestHttpClient.hpp"
#include "beatnext/quest/QuestMapInstaller.hpp"
#include "beatnext/quest/SongDetailsCatalog.hpp"
#include "beatnext/services/AtomicJsonCache.hpp"
#include "beatnext/services/BeatSaverCatalog.hpp"
#include "beatnext/services/RetryingHttpClient.hpp"
#include "beatnext/services/WorkerQueue.hpp"
#include "beatnext/services/YouTubeMusicProvider.hpp"

#include <filesystem>
#include <functional>
#include <mutex>
#include <optional>
#include <set>

namespace GlobalNamespace {
class BeatmapLevel;
}

namespace beatnext::quest {

class CompositionRoot {
  public:
    using StateCallback = std::function<void(const RecommendationSessionState&)>;
    using PrepareCallback = std::function<void(Outcome<std::string>)>;

    static CompositionRoot& instance();
    void initialize();
    void shutdown();

    void beginLevel(GlobalNamespace::BeatmapLevel* level);
    [[nodiscard]] RecommendationSessionState state() const;
    [[nodiscard]] std::uint64_t subscribe(StateCallback callback);
    void unsubscribe(std::uint64_t subscription);
    void select(std::size_t index);
    void prepare(std::size_t index, PrepareCallback callback);
    void rememberPlayed(const std::string& hash);

    [[nodiscard]] bool showNextOnResults() const;
    [[nodiscard]] bool showNextOnPause() const;
    void setShowNextOnResults(bool value);
    void setShowNextOnPause(bool value);

  private:
    CompositionRoot();
    ~CompositionRoot();
    CompositionRoot(const CompositionRoot&) = delete;
    CompositionRoot& operator=(const CompositionRoot&) = delete;

    [[nodiscard]] RecommendationRequest request() const;
    void loadDisplaySettings();
    void saveDisplaySettings() const;
    static void dispatch(std::function<void()> callback);

    std::filesystem::path dataRoot_;
    QuestHttpClient transport_;
    RetryingHttpClient http_;
    AtomicJsonCache mapCache_;
    AtomicJsonCache musicCache_;
    YouTubeMusicProvider music_;
    BeatSaverCatalog beatSaverCatalog_;
    SongDetailsCatalog catalog_;
    QuestMapInstaller mapInstaller_;
    RecommendationEngine engine_;
    WorkerQueue workers_;

    mutable std::mutex stateMutex_;
    RecommendationSession session_;
    std::set<std::string> playedHashes_;
    std::shared_ptr<CancellationSource> sessionCancellation_;
    bool showNextOnResults_{true};
    bool showNextOnPause_{true};
    bool initialized_{false};
};

} // namespace beatnext::quest
