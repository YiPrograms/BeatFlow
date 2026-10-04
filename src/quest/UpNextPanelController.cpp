#include "beatnext/quest/UpNextPanelController.hpp"

#include "beatnext/quest/Assets.hpp"
#include "beatnext/quest/Logger.hpp"
#include "beatnext/quest/RecommendationPresentation.hpp"
#include "beatnext/quest/RecommendationPreviewPlayer.hpp"
#include "beatnext/quest/SongSelectionNavigator.hpp"
#include "beatnext/quest/UpNextListCell.hpp"

#include "GlobalNamespace/MainFlowCoordinator.hpp"
#include "GlobalNamespace/SoloFreePlayFlowCoordinator.hpp"
#include "HMUI/ScrollView.hpp"
#include "HMUI/Touchable.hpp"
#include "TMPro/TextOverflowModes.hpp"
#include "UnityEngine/Canvas.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Object.hpp"
#include "UnityEngine/Quaternion.hpp"
#include "UnityEngine/Transform.hpp"
#include "UnityEngine/Vector2.hpp"
#include "UnityEngine/Vector3.hpp"
#include "beatsaber-hook/shared/utils/il2cpp-utils.hpp"
#include "bsml/shared/BSML.hpp"
#include "bsml/shared/BSML/FloatingScreen/FloatingScreen.hpp"
#include "bsml/shared/BSML/MainThreadScheduler.hpp"
#include "bsml/shared/Helpers/getters.hpp"
#include "bsml/shared/Helpers/utilities.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <sstream>

DEFINE_TYPE(beatnext::quest, UpNextPanelController);

namespace beatnext::quest {
namespace {

SafePtrUnity<BSML::FloatingScreen> resultsScreen;
SafePtrUnity<BSML::FloatingScreen> pauseScreen;
SongSelectionNavigator navigator;

constexpr std::string_view CellReuseIdentifier = "BeatNextRecommendationCell";
constexpr UnityEngine::Vector2 PanelSize{148.0F, 146.0F};
constexpr float PanelScale = 0.022F;
constexpr float FallbackPanelZ = 2.8F;
constexpr float ResultsFallbackY = 1.85F;
constexpr float PauseFallbackY = 1.95F;
constexpr float ResultsYOffset = 0.35F;
constexpr float PauseYOffset = 1.15F;
constexpr float RadiansToDegrees = 57.2957795F;
constexpr float DegreesToRadians = 0.0174532925F;
constexpr float ResultsAngleOffset = 52.0F;
constexpr float PauseAngleOffset = 64.0F;

struct PanelPlacement {
    UnityEngine::Vector3 position;
    UnityEngine::Quaternion rotation;
};

UnityEngine::Quaternion facePlayer(const UnityEngine::Vector3& position) {
    const float yaw = std::atan2(position.x, position.z) * RadiansToDegrees;
    return UnityEngine::Quaternion::Euler(0.0F, yaw, 0.0F);
}

PanelPlacement placeBeside(UnityEngine::Transform* anchor, bool pause) {
    const float angle = (pause ? PauseAngleOffset : ResultsAngleOffset) * DegreesToRadians;
    UnityEngine::Vector3 position(std::sin(angle) * FallbackPanelZ, pause ? PauseFallbackY : ResultsFallbackY,
                                  std::cos(angle) * FallbackPanelZ);
    if (anchor != nullptr) {
        const auto anchorPosition = anchor->get_position();
        const float radius = std::hypot(anchorPosition.x, anchorPosition.z);
        if (radius > 0.5F) {
            const float anchorAngle = std::atan2(anchorPosition.x, anchorPosition.z);
            const float panelAngle = anchorAngle + angle;
            position.x = std::sin(panelAngle) * radius;
            position.z = std::cos(panelAngle) * radius;
        }
        position.y = anchorPosition.y + (pause ? PauseYOffset : ResultsYOffset);
    }
    return {position, facePlayer(position)};
}

void followHost(BSML::FloatingScreen* screen, UnityEngine::Transform* anchor, bool pause,
                int framesRemaining) {
    if (screen == nullptr || anchor == nullptr || framesRemaining <= 0)
        return;
    SafePtrUnity<BSML::FloatingScreen> safeScreen(screen);
    SafePtrUnity<UnityEngine::Transform> safeAnchor(anchor);
    BSML::MainThreadScheduler::ScheduleNextFrame([safeScreen, safeAnchor, pause, framesRemaining]() mutable {
        if (!safeScreen || !safeAnchor)
            return;
        const auto placement = placeBeside(safeAnchor.ptr(), pause);
        safeScreen.ptr()->get_transform()->set_position(placement.position);
        safeScreen.ptr()->get_transform()->set_rotation(placement.rotation);
        followHost(safeScreen.ptr(), safeAnchor.ptr(), pause, framesRemaining - 1);
    });
}

void setText(TMPro::TextMeshProUGUI* target, const std::string& value) {
    if (target != nullptr)
        target->set_text(il2cpp_utils::newcsstr(value));
}

bool isInstalled(const RecommendationItemState& item) {
    return item.recommendation.installed || item.status == RecommendationItemStatus::Installed;
}

std::string listFingerprint(const RecommendationSessionState& state) {
    std::ostringstream value;
    value << state.generation;
    for (const auto& item : state.items) {
        value << '|' << item.recommendation.map.hash << ':' << static_cast<int>(item.status) << ':'
              << item.recommendation.installed << ':' << item.message;
    }
    return value.str();
}

std::string progressText(const RecommendationSessionState& state) {
    switch (state.progressStage) {
    case RecommendationProgressStage::ResolvingCurrentSong:
        return "Identifying this song on YouTube Music…";
    case RecommendationProgressStage::LoadingRadio:
        return state.sourceTrack ? "Loading radio for\n" + state.sourceTrack->title + "…"
                                 : "Loading YouTube Music radio…";
    case RecommendationProgressStage::MatchingMaps:
        return "Matched " + std::to_string(state.matchesFound) + " maps\nChecked " +
               std::to_string(state.completedTracks) + " of " + std::to_string(state.totalTracks) +
               " recommended songs";
    }
    return "Finding recommendations…";
}

bool safeYouTubeId(const std::string& providerId) {
    return !providerId.empty() && std::ranges::all_of(providerId, [](unsigned char character) {
        return std::isalnum(character) != 0 || character == '-' || character == '_';
    });
}

void afterMainMenuReady(std::function<void()> action, int attemptsRemaining = 300) {
    BSML::MainThreadScheduler::ScheduleNextFrame([action = std::move(action), attemptsRemaining]() mutable {
        auto* main = BSML::Helpers::GetMainFlowCoordinator();
        auto current = main == nullptr ? UnityW<HMUI::FlowCoordinator>(nullptr)
                                       : main->YoungestChildFlowCoordinatorOrSelf();
        const bool ready = main != nullptr && current == main && main->get_isActivated() &&
                           !main->get_isInTransition() &&
                           UnityEngine::GameObject::Find("SoloButton") != nullptr;
        if (ready) {
            action();
        } else if (attemptsRemaining > 0) {
            afterMainMenuReady(std::move(action), attemptsRemaining - 1);
        } else {
            logger.error("Beat Saber's main menu did not become ready for BeatNext navigation");
        }
    });
}

void leaveSongPickerThen(std::function<void()> action, int attemptsRemaining = 300) {
    BSML::MainThreadScheduler::ScheduleNextFrame([action = std::move(action), attemptsRemaining]() mutable {
        auto* main = BSML::Helpers::GetMainFlowCoordinator();
        auto current = main == nullptr ? UnityW<HMUI::FlowCoordinator>(nullptr)
                                       : main->YoungestChildFlowCoordinatorOrSelf();
        if (main != nullptr && current == main && main->get_isActivated() && !main->get_isInTransition()) {
            afterMainMenuReady(std::move(action));
            return;
        }

        auto* solo =
            BSML::Helpers::GetDiContainer()->Resolve<GlobalNamespace::SoloFreePlayFlowCoordinator*>();
        if (solo != nullptr && current == solo && solo->get_isActivated() && !solo->get_isInTransition()) {
            logger.info("Closing the existing Solo picker before opening the recommended map");
            solo->HandleScreenSystemBackButtonWasPressed();
            afterMainMenuReady(std::move(action));
            return;
        }

        if (attemptsRemaining > 0) {
            leaveSongPickerThen(std::move(action), attemptsRemaining - 1);
        } else {
            logger.error("The existing Solo picker did not become ready to close");
        }
    });
}

void openInSolo(const std::string& hash) {
    navigator.open(hash, [](Outcome<bool> opened) {
        if (!opened)
            logger.error("Could not open the BeatNext map in Solo: {}", opened.error().message);
    });
}

void returnToMainAndOpen(const std::string& hash) {
    leaveSongPickerThen([hash] { openInSolo(hash); });
}

BSML::FloatingScreen* createScreen(const char* name, bool pause,
                                   GlobalNamespace::PauseMenuManager* pauseManager,
                                   GlobalNamespace::ResultsViewController* resultsView) {
    UnityEngine::Transform* anchor = nullptr;
    if (pauseManager != nullptr)
        anchor = pauseManager->____pauseContainerTransform;
    else if (resultsView != nullptr)
        anchor = resultsView->get_transform();
    const auto placement = placeBeside(anchor, pause);
    auto* screen = BSML::FloatingScreen::CreateFloatingScreen(PanelSize, false, placement.position,
                                                              placement.rotation, 0.0F, false);
    screen->get_transform()->set_localScale(UnityEngine::Vector3(PanelScale, PanelScale, PanelScale));
    screen->get_gameObject()->set_name(il2cpp_utils::newcsstr(name));
    if (auto* canvas = screen->GetComponent<UnityEngine::Canvas*>())
        canvas->set_sortingOrder(31);
    auto* panel = screen->get_gameObject()->AddComponent<UpNextPanelController*>();
    panel->bind(pause, pauseManager, resultsView);
    followHost(screen, anchor, pause, 45);
    logger.info("Created {} panel at ({:.2f}, {:.2f}, {:.2f})", pause ? "pause" : "results",
                placement.position.x, placement.position.y, placement.position.z);
    return screen;
}

UpNextListCell* makeCell(HMUI::TableView* tableView) {
    auto tableCell = tableView->DequeueReusableCellForIdentifier(il2cpp_utils::newcsstr(CellReuseIdentifier));
    if (tableCell == nullptr) {
        tableCell = UnityEngine::GameObject::New_ctor("BeatNext Recommendation Cell")
                        ->AddComponent<UpNextListCell*>();
        tableCell->set_interactable(true);
        tableCell->set_reuseIdentifier(il2cpp_utils::newcsstr(CellReuseIdentifier));
        BSML::parse_and_construct(Assets::UpNextListCell_bsml, tableCell->get_transform(), tableCell);
        tableCell->get_gameObject()->AddComponent<HMUI::Touchable*>();
        auto cell = tableCell.cast<UpNextListCell>();
        cell->difficultyTexts =
            cell->difficultiesContainer->GetComponentsInChildren<TMPro::TextMeshProUGUI*>();
    }
    return tableCell.cast<UpNextListCell>();
}

} // namespace

void UpNextPanelController::ctor() {
    INVOKE_CTOR();
    pauseContext = false;
    subscription = 0;
    loadedArtworkUrl = nullptr;
    renderedListFingerprint = nullptr;
}

void UpNextPanelController::bind(bool isPause, GlobalNamespace::PauseMenuManager* pause,
                                 GlobalNamespace::ResultsViewController* results) {
    pauseContext = isPause;
    pauseManager = pause;
    resultsView = results;
    BSML::parse_and_construct(Assets::UpNextPanel_bsml, get_transform(), this);
    if (songList != nullptr)
        songList->tableView->SetDataSource(reinterpret_cast<HMUI::TableView::IDataSource*>(this), false);
    SafePtrUnity<UpNextPanelController> panel(this);
    subscription = CompositionRoot::instance().subscribe([panel](const RecommendationSessionState& state) {
        if (panel)
            panel.ptr()->render(state);
    });
}

void UpNextPanelController::OnDestroy() {
    recommendation_preview::stop();
    if (subscription != 0) {
        CompositionRoot::instance().unsubscribe(subscription);
        subscription = 0;
    }
}

float UpNextPanelController::CellSize() {
    return 14.0F;
}

int UpNextPanelController::NumberOfCells() {
    const auto count = CompositionRoot::instance().state().items.size();
    return static_cast<int>(
        std::min<std::size_t>(count, static_cast<std::size_t>(std::numeric_limits<int>::max())));
}

HMUI::TableCell* UpNextPanelController::CellForIdx(HMUI::TableView* tableView, int index) {
    const auto state = CompositionRoot::instance().state();
    if (index < 0 || static_cast<std::size_t>(index) >= state.items.size())
        return makeCell(tableView);
    return makeCell(tableView)->populate(state.items[static_cast<std::size_t>(index)]);
}

void UpNextPanelController::SelectSong(UnityW<HMUI::TableView> table, int index) {
    if (table == nullptr || index < 0)
        return;
    const auto state = CompositionRoot::instance().state();
    if (static_cast<std::size_t>(index) >= state.items.size())
        return;
    CompositionRoot::instance().select(static_cast<std::size_t>(index));
    const auto& recommendation = state.items[static_cast<std::size_t>(index)].recommendation;
    recommendation_preview::play(recommendation);
}

void UpNextPanelController::render(const RecommendationSessionState& state) {
    setText(headingText, state.stale ? "BeatNext · Cached/offline" : "BeatNext");
    if (state.sourceTrack) {
        setText(currentTrackText, state.sourceTrack->title);
    } else {
        setText(currentTrackText, "Matching this song…");
    }
    if (youtubeButton != nullptr) {
        youtubeButton->get_gameObject()->set_active(state.sourceTrack &&
                                                    safeYouTubeId(state.sourceTrack->providerId));
    }

    // Empty-state details are rendered in the panel body. Repeating them in the
    // header makes errors and no-match messages appear twice.
    if (!state.loading && !state.error && !state.items.empty())
        setText(statusText, std::to_string(state.items.size()) + " recommendations");
    else
        setText(statusText, "");

    const bool showContent = !state.loading && !state.items.empty();
    if (contentContainer != nullptr)
        contentContainer->set_active(showContent);
    if (loadingContainer != nullptr)
        loadingContainer->set_active(!showContent);
    if (loadingSpinner != nullptr)
        loadingSpinner->set_active(state.loading);
    if (state.loading)
        setText(loadingProgressText, progressText(state));
    else if (state.error)
        setText(loadingProgressText, state.error->message);
    else if (state.items.empty())
        setText(loadingProgressText, "No confident BeatSaver matches were found.");

    const auto fingerprint = listFingerprint(state);
    const std::string previousFingerprint =
        renderedListFingerprint ? static_cast<std::string>(renderedListFingerprint) : "";
    if (songList != nullptr && songList->tableView != nullptr && fingerprint != previousFingerprint) {
        renderedListFingerprint = il2cpp_utils::newcsstr(fingerprint);
        const auto previousPosition = songList->tableView->_scrollView->get_position();
        songList->tableView->ReloadData();
        songList->tableView->_scrollView->ScrollTo(previousPosition, false);
        if (state.selectedIndex && *state.selectedIndex < state.items.size()) {
            songList->tableView->SelectCellWithIdx(static_cast<int>(*state.selectedIndex), false);
        }
    }

    if (!state.selectedIndex || *state.selectedIndex >= state.items.size()) {
        setText(detailTitleText, state.loading ? "Preparing BeatNext" : "Select a recommendation");
        setText(detailArtistText, "");
        setText(detailMapText, "");
        setText(detailMetaText, "");
        setText(detailDifficultyText, "");
        if (actionButton != nullptr)
            actionButton->get_gameObject()->set_active(false);
        if (detailImage != nullptr)
            detailImage->get_gameObject()->set_active(false);
        loadedArtworkUrl = nullptr;
        return;
    }

    const auto& item = state.items[*state.selectedIndex];
    const auto& recommendation = item.recommendation;
    setText(detailTitleText, recommendation.track.title);
    setText(detailArtistText, presentation::artists(recommendation.track));
    setText(detailMapText,
            "BeatSaver: " + recommendation.map.songTitle + " · " + recommendation.map.songArtist);
    setText(detailMetaText, presentation::mapMetadata(recommendation));
    const auto difficultyText = item.status == RecommendationItemStatus::Failed
                                    ? item.message + " Select Retry download to try again."
                                    : presentation::difficulties(recommendation);
    setText(detailDifficultyText, difficultyText);
    if (detailImage != nullptr) {
        const auto& image = recommendation.map.coverUrl.empty() ? recommendation.track.artworkUrl
                                                                : recommendation.map.coverUrl;
        detailImage->get_gameObject()->set_active(!image.empty());
        const std::string loaded = loadedArtworkUrl ? static_cast<std::string>(loadedArtworkUrl) : "";
        if (!image.empty() && image != loaded) {
            loadedArtworkUrl = il2cpp_utils::newcsstr(image);
            BSML::Utilities::SetImage(detailImage, il2cpp_utils::newcsstr(image), true);
        } else if (image.empty()) {
            loadedArtworkUrl = nullptr;
        }
    }
    if (actionButton != nullptr) {
        const bool anotherDownload = std::ranges::any_of(state.items, [](const auto& candidate) {
            return candidate.status == RecommendationItemStatus::Downloading;
        });
        actionButton->get_gameObject()->set_active(true);
        actionButton->set_interactable(!anotherDownload);
        const auto text = item.status == RecommendationItemStatus::Downloading ? "Downloading…"
                          : item.status == RecommendationItemStatus::Failed    ? "Retry download"
                          : isInstalled(item)                                  ? "Play"
                                                                               : "Download";
        setText(actionButton->GetComponentInChildren<TMPro::TextMeshProUGUI*>(), text);
    }
}

void UpNextPanelController::OpenYouTubeMusic() {
    const auto state = CompositionRoot::instance().state();
    if (!state.sourceTrack || !safeYouTubeId(state.sourceTrack->providerId))
        return;
    static auto openUrl = il2cpp_utils::resolve_icall<void, StringW>("UnityEngine.Application::OpenURL");
    openUrl(il2cpp_utils::newcsstr("https://music.youtube.com/watch?v=" + state.sourceTrack->providerId));
}

void UpNextPanelController::Action() {
    const auto state = CompositionRoot::instance().state();
    if (!state.selectedIndex || *state.selectedIndex >= state.items.size())
        return;
    const auto index = *state.selectedIndex;
    const auto& item = state.items[index];
    if (item.status == RecommendationItemStatus::Downloading)
        return;
    if (isInstalled(item)) {
        const auto& hash = item.recommendation.map.hash;
        if (pauseContext) {
            pendingHash = il2cpp_utils::newcsstr(hash);
            if (confirmModal != nullptr)
                confirmModal->Show(true, true, nullptr);
            return;
        }
        if (resultsView != nullptr)
            resultsView->ContinueButtonPressed();
        up_next_ui::hideResults();
        returnToMainAndOpen(hash);
        return;
    }

    SafePtrUnity<UpNextPanelController> panel(this);
    CompositionRoot::instance().prepare(index, [panel](Outcome<std::string> prepared) mutable {
        if (panel && prepared)
            logger.info("Downloaded BeatNext map {}; waiting for the user to press Play", prepared.value());
    });
}

void UpNextPanelController::ConfirmExit() {
    if (confirmModal != nullptr)
        confirmModal->Hide(true, nullptr);
    if (!pendingHash || pendingHash->get_Length() == 0)
        return;
    const std::string hash = pendingHash;
    pendingHash = nullptr;
    if (pauseManager != nullptr)
        pauseManager->MenuButtonPressed();
    up_next_ui::hidePause();
    returnToMainAndOpen(hash);
}

void UpNextPanelController::CancelExit() {
    pendingHash = nullptr;
    if (confirmModal != nullptr)
        confirmModal->Hide(true, nullptr);
}

namespace up_next_ui {
void showResults(GlobalNamespace::ResultsViewController* results) {
    hideResults();
    resultsScreen = createScreen("BeatNext Results", false, nullptr, results);
}
void hideResults() {
    if (resultsScreen)
        UnityEngine::Object::Destroy(resultsScreen.ptr()->get_gameObject());
    resultsScreen = nullptr;
}
void showPause(GlobalNamespace::PauseMenuManager* pause) {
    hidePause();
    pauseScreen = createScreen("BeatNext Pause", true, pause, nullptr);
}
void hidePause() {
    if (pauseScreen)
        UnityEngine::Object::Destroy(pauseScreen.ptr()->get_gameObject());
    pauseScreen = nullptr;
}
} // namespace up_next_ui

} // namespace beatnext::quest
