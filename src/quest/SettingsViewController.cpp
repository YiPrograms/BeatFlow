#include "beatnext/quest/SettingsViewController.hpp"
#include "beatnext/quest/Assets.hpp"
#include "beatnext/quest/CompositionRoot.hpp"
#include "bsml/shared/BSML.hpp"

DEFINE_TYPE(beatnext::quest, SettingsViewController);
namespace beatnext::quest {
void SettingsViewController::ctor() {
    INVOKE_CTOR();
}
void SettingsViewController::DidActivate(bool firstActivation, bool, bool) {
    if (firstActivation)
        BSML::parse_and_construct(Assets::Settings_bsml, get_transform(), this);
}
bool SettingsViewController::get_showOnScore() {
    return CompositionRoot::instance().showNextOnResults();
}
void SettingsViewController::set_showOnScore(bool value) {
    CompositionRoot::instance().setShowNextOnResults(value);
}
bool SettingsViewController::get_showOnPause() {
    return CompositionRoot::instance().showNextOnPause();
}
void SettingsViewController::set_showOnPause(bool value) {
    CompositionRoot::instance().setShowNextOnPause(value);
}
} // namespace beatnext::quest
