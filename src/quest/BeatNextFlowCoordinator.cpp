#include "beatnext/quest/BeatNextFlowCoordinator.hpp"
#include "UnityEngine/Object.hpp"
#include "beatnext/quest/UI.hpp"
#include "bsml/shared/BSML.hpp"
#include "bsml/shared/Helpers/creation.hpp"
#include "bsml/shared/Helpers/getters.hpp"

DEFINE_TYPE(beatnext::quest, BeatNextFlowCoordinator);
namespace beatnext::quest {
void BeatNextFlowCoordinator::Awake() {
    if (settingsView == nullptr)
        settingsView = BSML::Helpers::CreateViewController<SettingsViewController*>();
    UnityEngine::Object::DontDestroyOnLoad(get_gameObject());
}
void BeatNextFlowCoordinator::DidActivate(bool firstActivation, bool, bool) {
    if (!firstActivation)
        return;
    SetTitle("BeatNext", HMUI::ViewController::AnimationType::In);
    showBackButton = true;
    ProvideInitialViewControllers(settingsView, nullptr, nullptr, nullptr, nullptr);
}
void BeatNextFlowCoordinator::BackButtonWasPressed(HMUI::ViewController*) {
    ui::close();
}
} // namespace beatnext::quest

namespace beatnext::quest::ui {
namespace {
SafePtrUnity<BeatNextFlowCoordinator> flow;
}
void initialize() {
    BSML::Register::RegisterMenuButton("BeatNext", "Configure Up Next recommendations", [] {
        if (!flow)
            flow = BSML::Helpers::CreateFlowCoordinator<BeatNextFlowCoordinator*>();
        auto parent = BSML::Helpers::GetMainFlowCoordinator()->YoungestChildFlowCoordinatorOrSelf();
        parent->PresentFlowCoordinator(flow.ptr(), nullptr,
                                       HMUI::ViewController::AnimationDirection::Horizontal, false, false);
    });
}
void close(bool immediately) {
    if (flow && flow->get_isActiveAndEnabled() && flow->get_isActivated() &&
        flow->____parentFlowCoordinator != nullptr) {
        flow->____parentFlowCoordinator->DismissFlowCoordinator(
            flow.ptr(), HMUI::ViewController::AnimationDirection::Horizontal, nullptr, immediately);
    }
}
} // namespace beatnext::quest::ui
