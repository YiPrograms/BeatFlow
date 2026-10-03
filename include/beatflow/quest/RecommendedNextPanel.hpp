#pragma once

#include "beatflow/quest/CustomTypeMacros.hpp"

#include "GlobalNamespace/ResultsViewController.hpp"
#include "TMPro/TextMeshProUGUI.hpp"
#include "UnityEngine/MonoBehaviour.hpp"
#include "UnityEngine/UI/Button.hpp"
#include "UnityEngine/UI/Image.hpp"

DECLARE_CLASS_CODEGEN(beatflow::quest, RecommendedNextPanel, UnityEngine::MonoBehaviour) {
    DECLARE_CTOR(ctor);
    DECLARE_INSTANCE_FIELD(UnityW<TMPro::TextMeshProUGUI>, headingText);
    DECLARE_INSTANCE_FIELD(UnityW<UnityEngine::UI::Button>, item0Button);
    DECLARE_INSTANCE_FIELD(UnityW<UnityEngine::UI::Button>, item1Button);
    DECLARE_INSTANCE_FIELD(UnityW<UnityEngine::UI::Button>, item2Button);
    DECLARE_INSTANCE_FIELD(UnityW<UnityEngine::UI::Button>, seeMoreButton);
    DECLARE_INSTANCE_FIELD(UnityW<UnityEngine::UI::Image>, item0Image);
    DECLARE_INSTANCE_FIELD(UnityW<UnityEngine::UI::Image>, item1Image);
    DECLARE_INSTANCE_FIELD(UnityW<UnityEngine::UI::Image>, item2Image);
    DECLARE_INSTANCE_FIELD(UnityW<GlobalNamespace::ResultsViewController>, resultsView);

    DECLARE_INSTANCE_METHOD(void, Select0);
    DECLARE_INSTANCE_METHOD(void, Select1);
    DECLARE_INSTANCE_METHOD(void, Select2);
    DECLARE_INSTANCE_METHOD(void, SeeMore);

  public:
    void bind(GlobalNamespace::ResultsViewController * results);
    void render();
    void select(std::size_t index);
};
