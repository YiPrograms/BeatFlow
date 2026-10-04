#include "beatnext/quest/UpNextPanelController.hpp"

#include "beatnext/quest/Assets.hpp"
#include "beatnext/quest/Logger.hpp"
#include "beatnext/quest/SongSelectionNavigator.hpp"

#include "UnityEngine/Canvas.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Object.hpp"
#include "UnityEngine/Quaternion.hpp"
#include "UnityEngine/Vector2.hpp"
#include "UnityEngine/Vector3.hpp"
#include "beatsaber-hook/shared/utils/il2cpp-utils.hpp"
#include "bsml/shared/BSML.hpp"
#include "bsml/shared/BSML/FloatingScreen/FloatingScreen.hpp"
#include "bsml/shared/BSML/MainThreadScheduler.hpp"
#include "bsml/shared/Helpers/getters.hpp"
#include "bsml/shared/Helpers/utilities.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <sstream>

DEFINE_TYPE(beatnext::quest, UpNextPanelController);

namespace beatnext::quest {
namespace {

SafePtrUnity<BSML::FloatingScreen> resultsScreen;
SafePtrUnity<BSML::FloatingScreen> pauseScreen;
SongSelectionNavigator navigator;

void setText(TMPro::TextMeshProUGUI* target, const std::string& value) {
    if (target != nullptr)
        target->set_text(il2cpp_utils::newcsstr(value));
}

std::string artists(const Track& track) {
    if (track.artists.empty())
        return "Unknown artist";
    std::ostringstream value;
    for (std::size_t index = 0; index < track.artists.size(); ++index) {
        if (index != 0)
            value << " · ";
        value << track.artists[index];
    }
    return value.str();
}

std::string difficulties(const RecommendedMap& recommendation) {
    std::ostringstream value;
    for (std::size_t index = 0; index < recommendation.playableDifficulties.size(); ++index) {
        if (index != 0)
            value << " · ";
        value << toString(recommendation.playableDifficulties[index].difficulty);
    }
    return value.str();
}

void afterMenuReady(std::function<void()> action, int attemptsRemaining = 300) {
    BSML::MainThreadScheduler::ScheduleNextFrame([action = std::move(action), attemptsRemaining]() mutable {
        auto current = BSML::Helpers::GetMainFlowCoordinator()->YoungestChildFlowCoordinatorOrSelf();
        const bool ready = current != nullptr && current->get_isActivated() &&
                           !current->get_isInTransition() &&
                           UnityEngine::GameObject::Find("SoloButton") != nullptr;
        if (ready) {
            action();
        } else if (attemptsRemaining > 0) {
            afterMenuReady(std::move(action), attemptsRemaining - 1);
        } else {
            logger.error("Beat Saber's main menu did not become ready for Up Next navigation");
        }
    });
}

void openInSolo(const std::string& hash) {
    navigator.open(hash, [](Outcome<bool> opened) {
        if (!opened)
            logger.error("Could not open the Up Next map in Solo: {}", opened.error().message);
    });
}

BSML::FloatingScreen* createScreen(const char* name, bool pause,
                                   GlobalNamespace::PauseMenuManager* pauseManager,
                                   GlobalNamespace::ResultsViewController* resultsView) {
    auto* screen = BSML::FloatingScreen::CreateFloatingScreen(
        UnityEngine::Vector2(84.0F, 54.0F), false, UnityEngine::Vector3(2.75F, pause ? 1.45F : 1.65F, 2.8F),
        UnityEngine::Quaternion::Euler(0.0F, -28.0F, 0.0F), 0.0F, false);
    screen->get_gameObject()->set_name(il2cpp_utils::newcsstr(name));
    if (auto* canvas = screen->GetComponent<UnityEngine::Canvas*>())
        canvas->set_sortingOrder(31);
    auto* panel = screen->get_gameObject()->AddComponent<UpNextPanelController*>();
    panel->bind(pause, pauseManager, resultsView);
    return screen;
}

} // namespace

void UpNextPanelController::ctor() {
    INVOKE_CTOR();
    pauseContext = false;
    subscription = 0;
    loadedArtworkUrl = nullptr;
}

void UpNextPanelController::bind(bool isPause, GlobalNamespace::PauseMenuManager* pause,
                                 GlobalNamespace::ResultsViewController* results) {
    pauseContext = isPause;
    pauseManager = pause;
    resultsView = results;
    BSML::parse_and_construct(Assets::UpNextPanel_bsml, get_transform(), this);
    SafePtrUnity<UpNextPanelController> panel(this);
    subscription = CompositionRoot::instance().subscribe([panel](const RecommendationSessionState& state) {
        if (panel)
            panel.ptr()->render(state);
    });
}

void UpNextPanelController::OnDestroy() {
    if (subscription != 0) {
        CompositionRoot::instance().unsubscribe(subscription);
        subscription = 0;
    }
}

void UpNextPanelController::render(const RecommendationSessionState& state) {
    setText(headingText, state.stale ? "Up Next · Cached/offline" : "Up Next");
    if (state.loading)
        setText(statusText, "Finding and matching maps…");
    else if (state.error)
        setText(statusText, state.error->message);
    else if (state.items.empty())
        setText(statusText, "No confident BeatSaver matches were found.");
    else
        setText(statusText, std::to_string(state.items.size()) + " recommendations");

    const std::array<UnityEngine::UI::Button*, 20> buttons{
        item0Button,  item1Button,  item2Button,  item3Button,  item4Button,  item5Button,  item6Button,
        item7Button,  item8Button,  item9Button,  item10Button, item11Button, item12Button, item13Button,
        item14Button, item15Button, item16Button, item17Button, item18Button, item19Button};
    for (std::size_t index = 0; index < buttons.size(); ++index) {
        const bool visible = index < state.items.size();
        if (buttons[index] == nullptr)
            continue;
        buttons[index]->get_gameObject()->set_active(visible);
        if (!visible)
            continue;
        const auto& item = state.items[index];
        std::string label =
            "<b>" + item.recommendation.track.title + "</b>\n" + artists(item.recommendation.track);
        if (item.status == RecommendationItemStatus::Downloading)
            label += " · Downloading…";
        else if (item.status == RecommendationItemStatus::Installed)
            label += " · Installed";
        else if (item.status == RecommendationItemStatus::Failed)
            label += " · Retry";
        setText(buttons[index]->GetComponentInChildren<TMPro::TextMeshProUGUI*>(), label);
    }

    if (!state.selectedIndex || *state.selectedIndex >= state.items.size()) {
        setText(detailTitleText, state.loading ? "Preparing Up Next" : "Select a recommendation");
        setText(detailArtistText, "");
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
    setText(detailArtistText, artists(recommendation.track));
    setText(detailMetaText,
            "Mapped by " + recommendation.map.mapper + " · " +
                std::to_string(static_cast<int>(std::round(recommendation.map.rating * 100.0))) + "%");
    const auto difficultyText = item.status == RecommendationItemStatus::Failed
                                    ? item.message + " Select Retry download to try again."
                                    : difficulties(recommendation);
    setText(detailDifficultyText, difficultyText);
    if (detailImage != nullptr) {
        const auto& image = recommendation.track.artworkUrl.empty() ? recommendation.map.coverUrl
                                                                    : recommendation.track.artworkUrl;
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
                          : recommendation.installed || item.status == RecommendationItemStatus::Installed
                              ? "Open in Solo"
                              : "Download & Open";
        setText(actionButton->GetComponentInChildren<TMPro::TextMeshProUGUI*>(), text);
    }
}

void UpNextPanelController::Select0() {
    select(0);
}
void UpNextPanelController::Select1() {
    select(1);
}
void UpNextPanelController::Select2() {
    select(2);
}
void UpNextPanelController::Select3() {
    select(3);
}
void UpNextPanelController::Select4() {
    select(4);
}
void UpNextPanelController::Select5() {
    select(5);
}
void UpNextPanelController::Select6() {
    select(6);
}
void UpNextPanelController::Select7() {
    select(7);
}
void UpNextPanelController::Select8() {
    select(8);
}
void UpNextPanelController::Select9() {
    select(9);
}
void UpNextPanelController::Select10() {
    select(10);
}
void UpNextPanelController::Select11() {
    select(11);
}
void UpNextPanelController::Select12() {
    select(12);
}
void UpNextPanelController::Select13() {
    select(13);
}
void UpNextPanelController::Select14() {
    select(14);
}
void UpNextPanelController::Select15() {
    select(15);
}
void UpNextPanelController::Select16() {
    select(16);
}
void UpNextPanelController::Select17() {
    select(17);
}
void UpNextPanelController::Select18() {
    select(18);
}
void UpNextPanelController::Select19() {
    select(19);
}

void UpNextPanelController::select(std::size_t index) {
    CompositionRoot::instance().select(index);
}

void UpNextPanelController::Action() {
    const auto state = CompositionRoot::instance().state();
    if (!state.selectedIndex || *state.selectedIndex >= state.items.size())
        return;
    const auto index = *state.selectedIndex;
    SafePtrUnity<UpNextPanelController> panel(this);
    CompositionRoot::instance().prepare(index, [panel](Outcome<std::string> prepared) mutable {
        if (!panel || !prepared)
            return;
        const auto hash = std::move(prepared).value();
        if (panel.ptr()->pauseContext) {
            panel.ptr()->pendingHash = il2cpp_utils::newcsstr(hash);
            if (panel.ptr()->confirmModal != nullptr)
                panel.ptr()->confirmModal->Show(true, true, nullptr);
            return;
        }
        if (panel.ptr()->resultsView != nullptr)
            panel.ptr()->resultsView->ContinueButtonPressed();
        up_next_ui::hideResults();
        afterMenuReady([hash] { openInSolo(hash); });
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
    afterMenuReady([hash] { openInSolo(hash); });
}

void UpNextPanelController::CancelExit() {
    pendingHash = nullptr;
    if (confirmModal != nullptr)
        confirmModal->Hide(true, nullptr);
}

namespace up_next_ui {
void showResults(GlobalNamespace::ResultsViewController* results) {
    hideResults();
    resultsScreen = createScreen("BeatNext Results Up Next", false, nullptr, results);
}
void hideResults() {
    if (resultsScreen)
        UnityEngine::Object::Destroy(resultsScreen.ptr()->get_gameObject());
    resultsScreen = nullptr;
}
void showPause(GlobalNamespace::PauseMenuManager* pause) {
    hidePause();
    pauseScreen = createScreen("BeatNext Pause Up Next", true, pause, nullptr);
}
void hidePause() {
    if (pauseScreen)
        UnityEngine::Object::Destroy(pauseScreen.ptr()->get_gameObject());
    pauseScreen = nullptr;
}
} // namespace up_next_ui

} // namespace beatnext::quest
