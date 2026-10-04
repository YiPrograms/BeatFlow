#pragma once
#include "HMUI/ViewController.hpp"
#include "beatnext/quest/CustomTypeMacros.hpp"
#include "bsml/shared/macros.hpp"

DECLARE_CLASS_CODEGEN(beatnext::quest, SettingsViewController, HMUI::ViewController) {
    DECLARE_CTOR(ctor);
    DECLARE_OVERRIDE_METHOD_MATCH(void, DidActivate, &HMUI::ViewController::DidActivate, bool firstActivation,
                                  bool addedToHierarchy, bool screenSystemEnabling);
    DECLARE_BSML_PROPERTY(bool, showOnScore);
    DECLARE_BSML_PROPERTY(bool, showOnPause);
};
