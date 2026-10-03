#include "beatflow/quest/LevelLifecycle.hpp"

#include "beatflow/quest/CompositionRoot.hpp"
#include "beatflow/quest/Logger.hpp"
#include "beatflow/quest/RecommendedNextPanel.hpp"

#include "GlobalNamespace/LevelCompletionResults.hpp"
#include "GlobalNamespace/PlayerSpecificSettings.hpp"
#include "GlobalNamespace/ResultsViewController.hpp"
#include "GlobalNamespace/StandardLevelScenesTransitionSetupDataSO.hpp"
#include "UnityEngine/GameObject.hpp"
#include "beatsaber-hook/shared/utils/hooking.hpp"

namespace beatflow::quest {
namespace {

MAKE_HOOK_MATCH(StandardLevelStarted,
                &GlobalNamespace::StandardLevelScenesTransitionSetupDataSO::InitAndSetupScenes, void,
                GlobalNamespace::StandardLevelScenesTransitionSetupDataSO* self,
                GlobalNamespace::PlayerSpecificSettings* playerSpecificSettings, StringW backButtonText,
                bool startPaused) {
    CompositionRoot::instance().prefetchForLevel(self->get_beatmapLevel());
    StandardLevelStarted(self, playerSpecificSettings, backButtonText, startPaused);
}

MAKE_HOOK_MATCH(ResultsActivated, &GlobalNamespace::ResultsViewController::DidActivate, void,
                GlobalNamespace::ResultsViewController* self, bool firstActivation, bool addedToHierarchy,
                bool screenSystemEnabling) {
    ResultsActivated(self, firstActivation, addedToHierarchy, screenSystemEnabling);
    if (!firstActivation || self->____levelCompletionResults == nullptr ||
        self->____levelCompletionResults->___levelEndStateType.value__ != 1) {
        return;
    }
    auto* panel = self->get_gameObject()->AddComponent<RecommendedNextPanel*>();
    panel->bind(self);
}

} // namespace

void installLevelLifecycleHooks() {
    INSTALL_HOOK(logger, StandardLevelStarted);
    INSTALL_HOOK(logger, ResultsActivated);
}

} // namespace beatflow::quest
