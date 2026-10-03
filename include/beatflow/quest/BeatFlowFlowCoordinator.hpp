#pragma once

#include "beatflow/quest/CustomTypeMacros.hpp"
#include "beatflow/quest/ForYouViewController.hpp"

#include "HMUI/FlowCoordinator.hpp"

DECLARE_CLASS_CODEGEN(beatflow::quest, BeatFlowFlowCoordinator, HMUI::FlowCoordinator) {
    DECLARE_INSTANCE_FIELD(UnityW<beatflow::quest::ForYouViewController>, recommendationsView);
    DECLARE_INSTANCE_METHOD(void, Awake);
    DECLARE_OVERRIDE_METHOD_MATCH(void, DidActivate, &HMUI::FlowCoordinator::DidActivate,
                                  bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling);
    DECLARE_OVERRIDE_METHOD_MATCH(void, BackButtonWasPressed, &HMUI::FlowCoordinator::BackButtonWasPressed,
                                  HMUI::ViewController* topViewController);
};
