#pragma once

#include "beatflow/core/RecommendationEngine.hpp"
#include "beatflow/quest/QuestCredentialStore.hpp"
#include "beatflow/quest/QuestHttpClient.hpp"
#include "beatflow/quest/QuestSongLibrary.hpp"
#include "beatflow/quest/SongDetailsCatalog.hpp"
#include "beatflow/services/AtomicJsonCache.hpp"
#include "beatflow/services/BeatSaverCatalog.hpp"
#include "beatflow/services/OAuthClient.hpp"
#include "beatflow/services/RetryingHttpClient.hpp"
#include "beatflow/services/WorkerQueue.hpp"
#include "beatflow/services/YouTubeMusicProvider.hpp"

#include <filesystem>
#include <functional>
#include <mutex>
#include <optional>
#include <set>

namespace GlobalNamespace {
class BeatmapLevel;
}

namespace beatflow::quest {

struct AuthorizationViewState {
    std::string message;
    std::string verificationUrl;
    std::string userCode;
    bool connected{false};
    bool busy{false};
};

struct RecommendationViewState {
    std::vector<RecommendedMap> recommendations;
    std::optional<ServiceError> error;
    std::string context;
    bool loading{false};
    bool stale{false};
};

class CompositionRoot {
  public:
    using AuthorizationCallback = std::function<void(const AuthorizationViewState&)>;
    using RecommendationCallback = std::function<void(const RecommendationViewState&)>;
    using PrepareCallback = std::function<void(Outcome<std::string>)>;

    static CompositionRoot& instance();

    void initialize();
    void shutdown();

    void refreshForYou(RecommendationFilters filters, RecommendationCallback callback);
    void connect(AuthorizationCallback callback);
    [[nodiscard]] bool hasConnectedAccount();
    void cancelInteractive();
    Outcome<bool> disconnect();
    Outcome<bool> clearLocalData();

    void prefetchForLevel(GlobalNamespace::BeatmapLevel* level);
    [[nodiscard]] RecommendationViewState nextState() const;
    [[nodiscard]] RecommendationViewState browseState() const;
    void browseForYouRecommendations();
    void browseNextRecommendations();

    void prepare(const RecommendedMap& recommendation, PrepareCallback callback);
    Outcome<bool> openPrepared(const std::string& hash);
    void rememberPlayed(const std::string& hash);

    [[nodiscard]] RecommendationFilters filters() const;
    void setFilters(RecommendationFilters filters);
    [[nodiscard]] bool showNextOnResults() const;
    [[nodiscard]] bool showNextOnPause() const;
    void setShowNextOnResults(bool value);
    void setShowNextOnPause(bool value);

  private:
    CompositionRoot();
    ~CompositionRoot();
    CompositionRoot(const CompositionRoot&) = delete;
    CompositionRoot& operator=(const CompositionRoot&) = delete;

    [[nodiscard]] RecommendationRequest request(std::size_t maximumResults) const;
    void loadDisplaySettings();
    void saveDisplaySettings() const;
    static void dispatch(std::function<void()> callback);

    std::filesystem::path dataRoot_;
    QuestHttpClient transport_;
    RetryingHttpClient http_;
    AtomicJsonCache mapCache_;
    AtomicJsonCache musicCache_;
    AtomicJsonCache personalizedCache_;
    QuestCredentialStore credentials_;
    OAuthClient oauth_;
    YouTubeMusicProvider music_;
    BeatSaverCatalog beatSaverCatalog_;
    SongDetailsCatalog catalog_;
    QuestSongLibrary library_;
    RecommendationEngine engine_;
    WorkerQueue workers_;

    mutable std::mutex stateMutex_;
    RecommendationViewState forYouState_;
    RecommendationViewState nextState_;
    RecommendationViewState browseState_;
    enum class BrowseMode { ForYou, Next };
    BrowseMode browseMode_{BrowseMode::ForYou};
    RecommendationFilters filters_;
    std::set<std::string> playedHashes_;
    std::shared_ptr<CancellationSource> interactiveCancellation_;
    std::shared_ptr<CancellationSource> prefetchCancellation_;
    std::uint64_t interactiveGeneration_{0};
    std::uint64_t prefetchGeneration_{0};
    bool showNextOnResults_{true};
    bool showNextOnPause_{true};
    bool initialized_{false};
};

} // namespace beatflow::quest
