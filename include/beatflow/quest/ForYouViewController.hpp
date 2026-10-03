#pragma once

#include "beatflow/quest/CustomTypeMacros.hpp"

#include "HMUI/ViewController.hpp"
#include "TMPro/TextMeshProUGUI.hpp"
#include "UnityEngine/UI/Button.hpp"
#include "UnityEngine/UI/Image.hpp"
#include "bsml/shared/macros.hpp"

DECLARE_CLASS_CODEGEN(beatflow::quest, ForYouViewController, HMUI::ViewController) {
    DECLARE_CTOR(ctor);
    DECLARE_OVERRIDE_METHOD_MATCH(void, DidActivate, &HMUI::ViewController::DidActivate, bool firstActivation,
                                  bool addedToHierarchy, bool screenSystemEnabling);
    DECLARE_OVERRIDE_METHOD_MATCH(void, DidDeactivate, &HMUI::ViewController::DidDeactivate,
                                  bool removedFromHierarchy, bool screenSystemDisabling);

    DECLARE_INSTANCE_FIELD(UnityW<TMPro::TextMeshProUGUI>, connectionText);
    DECLARE_INSTANCE_FIELD(UnityW<TMPro::TextMeshProUGUI>, statusText);
    DECLARE_INSTANCE_FIELD(UnityW<TMPro::TextMeshProUGUI>, filterText);
    DECLARE_INSTANCE_FIELD(UnityW<TMPro::TextMeshProUGUI>, pageText);
    DECLARE_INSTANCE_FIELD(UnityW<UnityEngine::UI::Button>, card0Button);
    DECLARE_INSTANCE_FIELD(UnityW<UnityEngine::UI::Button>, card1Button);
    DECLARE_INSTANCE_FIELD(UnityW<UnityEngine::UI::Button>, card2Button);
    DECLARE_INSTANCE_FIELD(UnityW<UnityEngine::UI::Image>, card0Image);
    DECLARE_INSTANCE_FIELD(UnityW<UnityEngine::UI::Image>, card1Image);
    DECLARE_INSTANCE_FIELD(UnityW<UnityEngine::UI::Image>, card2Image);
    DECLARE_INSTANCE_FIELD(StringW, authorizationUrl);
    DECLARE_INSTANCE_FIELD(int32_t, page);
    DECLARE_INSTANCE_FIELD(int32_t, difficultyMode);
    DECLARE_INSTANCE_FIELD(int32_t, npsMode);
    DECLARE_BSML_PROPERTY(bool, showOnSongEnd);
    DECLARE_BSML_PROPERTY(bool, showOnPause);

    DECLARE_INSTANCE_METHOD(void, Refresh);
    DECLARE_INSTANCE_METHOD(void, Connect);
    DECLARE_INSTANCE_METHOD(void, OpenBrowser);
    DECLARE_INSTANCE_METHOD(void, Disconnect);
    DECLARE_INSTANCE_METHOD(void, ClearLocalData);
    DECLARE_INSTANCE_METHOD(void, Cancel);
    DECLARE_INSTANCE_METHOD(void, PreviousPage);
    DECLARE_INSTANCE_METHOD(void, NextPage);
    DECLARE_INSTANCE_METHOD(void, CycleDifficulty);
    DECLARE_INSTANCE_METHOD(void, CycleNps);
    DECLARE_INSTANCE_METHOD(void, Select0);
    DECLARE_INSTANCE_METHOD(void, Select1);
    DECLARE_INSTANCE_METHOD(void, Select2);

  public:
    void render();
    void select(std::size_t slot);
};
