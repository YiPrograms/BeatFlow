#include "beatnext/quest/LevelLifecycle.hpp"

#include "beatnext/quest/CompositionRoot.hpp"
#include "beatnext/quest/Logger.hpp"
#include "beatnext/quest/SongSelectionNavigator.hpp"
#include "beatnext/quest/UpNextPanelController.hpp"

#include "GlobalNamespace/BeatmapLevelPack.hpp"
#include "GlobalNamespace/LevelCompletionResults.hpp"
#include "GlobalNamespace/LevelFilteringNavigationController.hpp"
#include "GlobalNamespace/MultiplayerLevelScenesTransitionSetupDataSO.hpp"
#include "GlobalNamespace/PauseMenuManager.hpp"
#include "GlobalNamespace/PlayerSpecificSettings.hpp"
#include "GlobalNamespace/ResultsViewController.hpp"
#include "GlobalNamespace/SelectLevelCategoryViewController.hpp"
#include "GlobalNamespace/SongPackMask.hpp"
#include "GlobalNamespace/StandardLevelScenesTransitionSetupDataSO.hpp"
#include "beatsaber-hook/shared/utils/hooking.hpp"

namespace beatnext::quest {
namespace {

bool soloLevelActive = false;

MAKE_HOOK_MATCH(LevelFilteringSetup, &GlobalNamespace::LevelFilteringNavigationController::Setup, void,
                GlobalNamespace::LevelFilteringNavigationController* self,
                GlobalNamespace::SongPackMask songPackMask,
                GlobalNamespace::BeatmapLevelPack* levelPackToBeSelectedAfterPresent,
                GlobalNamespace::SelectLevelCategoryViewController_LevelCategory startLevelCategory,
                bool hidePacksIfOneOrNone, bool enableCustomLevels) {
    LevelFilteringSetup(self, songPackMask, levelPackToBeSelectedAfterPresent, startLevelCategory,
                        hidePacksIfOneOrNone, enableCustomLevels);
    if (song_selection_navigation::consumeCustomCategoryRequest()) {
        self->____selectLevelCategoryViewController->Setup(
            GlobalNamespace::SelectLevelCategoryViewController_LevelCategory::CustomSongs,
            self->____enabledLevelCategories);
        logger.info("Opened BeatNext selection in the Custom Songs category");
    }
}

MAKE_HOOK_MATCH(StandardLevelStarted,
                &GlobalNamespace::StandardLevelScenesTransitionSetupDataSO::InitAndSetupScenes, void,
                GlobalNamespace::StandardLevelScenesTransitionSetupDataSO* self,
                GlobalNamespace::PlayerSpecificSettings* playerSpecificSettings, StringW backButtonText,
                bool startPaused) {
    up_next_ui::hidePause();
    up_next_ui::hideResults();
    soloLevelActive = true;
    CompositionRoot::instance().beginLevel(self->get_beatmapLevel());
    StandardLevelStarted(self, playerSpecificSettings, backButtonText, startPaused);
}

MAKE_HOOK_MATCH(MultiplayerLevelStarted,
                &GlobalNamespace::MultiplayerLevelScenesTransitionSetupDataSO::InitAndSetupScenes, void,
                GlobalNamespace::MultiplayerLevelScenesTransitionSetupDataSO* self) {
    soloLevelActive = false;
    up_next_ui::hidePause();
    up_next_ui::hideResults();
    MultiplayerLevelStarted(self);
}

MAKE_HOOK_MATCH(ResultsActivated, &GlobalNamespace::ResultsViewController::DidActivate, void,
                GlobalNamespace::ResultsViewController* self, bool firstActivation, bool addedToHierarchy,
                bool screenSystemEnabling) {
    ResultsActivated(self, firstActivation, addedToHierarchy, screenSystemEnabling);
    const bool enabled = CompositionRoot::instance().showNextOnResults();
    const bool completed = self->____levelCompletionResults != nullptr &&
                           self->____levelCompletionResults->___levelEndStateType.value__ == 1;
    logger.info("Results screen activated: enabled={}, completed={}", enabled, completed);
    if (soloLevelActive && enabled && completed)
        up_next_ui::showResults(self);
}

MAKE_HOOK_MATCH(ResultsDeactivated, &GlobalNamespace::ResultsViewController::DidDeactivate, void,
                GlobalNamespace::ResultsViewController* self, bool removedFromHierarchy,
                bool screenSystemDisabling) {
    up_next_ui::hideResults();
    ResultsDeactivated(self, removedFromHierarchy, screenSystemDisabling);
}

MAKE_HOOK_MATCH(PauseMenuShown, &GlobalNamespace::PauseMenuManager::ShowMenu, void,
                GlobalNamespace::PauseMenuManager* self) {
    PauseMenuShown(self);
    const bool enabled = CompositionRoot::instance().showNextOnPause();
    logger.info("Pause screen shown: solo={}, enabled={}", soloLevelActive, enabled);
    if (soloLevelActive && enabled)
        up_next_ui::showPause(self);
}

MAKE_HOOK_MATCH(PauseContinued, &GlobalNamespace::PauseMenuManager::ContinueButtonPressed, void,
                GlobalNamespace::PauseMenuManager* self) {
    up_next_ui::hidePause();
    PauseContinued(self);
}

MAKE_HOOK_MATCH(PauseRestarted, &GlobalNamespace::PauseMenuManager::RestartButtonPressed, void,
                GlobalNamespace::PauseMenuManager* self) {
    up_next_ui::hidePause();
    PauseRestarted(self);
}

MAKE_HOOK_MATCH(PauseExited, &GlobalNamespace::PauseMenuManager::MenuButtonPressed, void,
                GlobalNamespace::PauseMenuManager* self) {
    up_next_ui::hidePause();
    PauseExited(self);
}

} // namespace

void installLevelLifecycleHooks() {
    INSTALL_HOOK(logger, LevelFilteringSetup);
    INSTALL_HOOK(logger, StandardLevelStarted);
    INSTALL_HOOK(logger, MultiplayerLevelStarted);
    INSTALL_HOOK(logger, ResultsActivated);
    INSTALL_HOOK(logger, ResultsDeactivated);
    INSTALL_HOOK(logger, PauseMenuShown);
    INSTALL_HOOK(logger, PauseContinued);
    INSTALL_HOOK(logger, PauseRestarted);
    INSTALL_HOOK(logger, PauseExited);
}

} // namespace beatnext::quest
