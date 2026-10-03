#include "beatflow/quest/LevelLifecycle.hpp"

#include "beatflow/quest/CompositionRoot.hpp"
#include "beatflow/quest/Logger.hpp"
#include "beatflow/quest/PauseRecommendedNextPanel.hpp"
#include "beatflow/quest/RecommendedNextPanel.hpp"

#include "GlobalNamespace/LevelCompletionResults.hpp"
#include "GlobalNamespace/MultiplayerLevelScenesTransitionSetupDataSO.hpp"
#include "GlobalNamespace/PauseMenuManager.hpp"
#include "GlobalNamespace/PlayerSpecificSettings.hpp"
#include "GlobalNamespace/ResultsViewController.hpp"
#include "GlobalNamespace/StandardLevelScenesTransitionSetupDataSO.hpp"
#include "UnityEngine/GameObject.hpp"
#include "beatsaber-hook/shared/utils/hooking.hpp"

namespace beatflow::quest {
namespace {

bool soloLevelActive = false;

MAKE_HOOK_MATCH(StandardLevelStarted,
                &GlobalNamespace::StandardLevelScenesTransitionSetupDataSO::InitAndSetupScenes, void,
                GlobalNamespace::StandardLevelScenesTransitionSetupDataSO* self,
                GlobalNamespace::PlayerSpecificSettings* playerSpecificSettings, StringW backButtonText,
                bool startPaused) {
    soloLevelActive = true;
    CompositionRoot::instance().prefetchForLevel(self->get_beatmapLevel());
    StandardLevelStarted(self, playerSpecificSettings, backButtonText, startPaused);
}

MAKE_HOOK_MATCH(MultiplayerLevelStarted,
                &GlobalNamespace::MultiplayerLevelScenesTransitionSetupDataSO::InitAndSetupScenes, void,
                GlobalNamespace::MultiplayerLevelScenesTransitionSetupDataSO* self) {
    soloLevelActive = false;
    MultiplayerLevelStarted(self);
}

MAKE_HOOK_MATCH(ResultsActivated, &GlobalNamespace::ResultsViewController::DidActivate, void,
                GlobalNamespace::ResultsViewController* self, bool firstActivation, bool addedToHierarchy,
                bool screenSystemEnabling) {
    ResultsActivated(self, firstActivation, addedToHierarchy, screenSystemEnabling);
    if (!firstActivation || !CompositionRoot::instance().showNextOnResults() ||
        self->____levelCompletionResults == nullptr ||
        self->____levelCompletionResults->___levelEndStateType.value__ != 1) {
        return;
    }
    auto* panel = self->get_gameObject()->AddComponent<RecommendedNextPanel*>();
    panel->bind(self);
}

MAKE_HOOK_MATCH(PauseMenuShown, &GlobalNamespace::PauseMenuManager::ShowMenu, void,
                GlobalNamespace::PauseMenuManager* self) {
    PauseMenuShown(self);
    if (!soloLevelActive || !CompositionRoot::instance().showNextOnPause() ||
        self->____pauseContainerTransform == nullptr) {
        return;
    }
    auto container = self->____pauseContainerTransform->get_gameObject();
    auto* panel = container->GetComponent<PauseRecommendedNextPanel*>();
    if (panel == nullptr) {
        panel = container->AddComponent<PauseRecommendedNextPanel*>();
        panel->bind();
    } else {
        panel->render();
    }
}

} // namespace

void installLevelLifecycleHooks() {
    INSTALL_HOOK(logger, StandardLevelStarted);
    INSTALL_HOOK(logger, MultiplayerLevelStarted);
    INSTALL_HOOK(logger, ResultsActivated);
    INSTALL_HOOK(logger, PauseMenuShown);
}

} // namespace beatflow::quest
