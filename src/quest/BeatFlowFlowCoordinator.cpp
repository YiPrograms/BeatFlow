#include "beatflow/quest/BeatFlowFlowCoordinator.hpp"

#include "beatflow/quest/CompositionRoot.hpp"
#include "beatflow/quest/UI.hpp"

#include "UnityEngine/Object.hpp"
#include "bsml/shared/BSML.hpp"
#include "bsml/shared/Helpers/creation.hpp"
#include "bsml/shared/Helpers/getters.hpp"

DEFINE_TYPE(beatflow::quest, BeatFlowFlowCoordinator);

namespace beatflow::quest {

void BeatFlowFlowCoordinator::Awake() {
    if (recommendationsView == nullptr) {
        recommendationsView = BSML::Helpers::CreateViewController<ForYouViewController*>();
    }
    UnityEngine::Object::DontDestroyOnLoad(get_gameObject());
}

void BeatFlowFlowCoordinator::DidActivate(bool firstActivation, bool, bool) {
    if (!firstActivation) {
        return;
    }
    SetTitle("BeatFlow", HMUI::ViewController::AnimationType::In);
    showBackButton = true;
    ProvideInitialViewControllers(recommendationsView, nullptr, nullptr, nullptr, nullptr);
}

void BeatFlowFlowCoordinator::BackButtonWasPressed(HMUI::ViewController*) {
    ui::close();
}

} // namespace beatflow::quest

namespace beatflow::quest::ui {
namespace {

SafePtrUnity<BeatFlowFlowCoordinator> flow;
bool nextMode = false;

void show(bool showNext) {
    nextMode = showNext;
    if (showNext) {
        CompositionRoot::instance().browseNextRecommendations();
    } else {
        CompositionRoot::instance().browseForYouRecommendations();
    }
    if (!flow) {
        flow = BSML::Helpers::CreateFlowCoordinator<BeatFlowFlowCoordinator*>();
    }
    auto parent = BSML::Helpers::GetMainFlowCoordinator()->YoungestChildFlowCoordinatorOrSelf();
    parent->PresentFlowCoordinator(flow.ptr(), nullptr, HMUI::ViewController::AnimationDirection::Horizontal,
                                   false, false);
}

} // namespace

void initialize() {
    BSML::Register::RegisterMenuButton("BeatFlow", "Play recommendations from YouTube Music and BeatSaver",
                                       [] { show(false); });
}

void showForYou() {
    show(false);
}

void showRecommendedNext() {
    show(true);
}

void close(bool immediately) {
    CompositionRoot::instance().cancelInteractive();
    if (flow && flow->get_isActiveAndEnabled() && flow->get_isActivated() &&
        flow->____parentFlowCoordinator != nullptr) {
        flow->____parentFlowCoordinator->DismissFlowCoordinator(
            flow.ptr(), HMUI::ViewController::AnimationDirection::Horizontal, nullptr, immediately);
    }
}

bool openingRecommendedNext() {
    return nextMode;
}

} // namespace beatflow::quest::ui
