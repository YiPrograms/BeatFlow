#pragma once

#include "beatflow/quest/CustomTypeMacros.hpp"

#include "TMPro/TextMeshProUGUI.hpp"
#include "UnityEngine/MonoBehaviour.hpp"

DECLARE_CLASS_CODEGEN(beatflow::quest, PauseRecommendedNextPanel, UnityEngine::MonoBehaviour) {
    DECLARE_CTOR(ctor);
    DECLARE_INSTANCE_FIELD(UnityW<TMPro::TextMeshProUGUI>, headingText);
    DECLARE_INSTANCE_FIELD(UnityW<TMPro::TextMeshProUGUI>, item0Text);
    DECLARE_INSTANCE_FIELD(UnityW<TMPro::TextMeshProUGUI>, item1Text);
    DECLARE_INSTANCE_FIELD(UnityW<TMPro::TextMeshProUGUI>, item2Text);

  public:
    void bind();
    void render();
};
