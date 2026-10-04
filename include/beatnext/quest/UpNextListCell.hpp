#pragma once

#include "beatnext/core/RecommendationSession.hpp"
#include "beatnext/quest/CustomTypeMacros.hpp"

#include "HMUI/ImageView.hpp"
#include "HMUI/TableCell.hpp"
#include "TMPro/TextMeshProUGUI.hpp"
#include "UnityEngine/UI/HorizontalOrVerticalLayoutGroup.hpp"

DECLARE_CLASS_CODEGEN(beatnext::quest, UpNextListCell, HMUI::TableCell) {
    DECLARE_CTOR(ctor);
    DECLARE_OVERRIDE_METHOD_MATCH(void, SelectionDidChange, &HMUI::SelectableCell::SelectionDidChange,
                                  HMUI::SelectableCell::TransitionType transitionType);
    DECLARE_OVERRIDE_METHOD_MATCH(void, HighlightDidChange, &HMUI::SelectableCell::HighlightDidChange,
                                  HMUI::SelectableCell::TransitionType transitionType);
    DECLARE_OVERRIDE_METHOD_MATCH(void, WasPreparedForReuse, &HMUI::TableCell::WasPreparedForReuse);
    DECLARE_INSTANCE_FIELD(UnityW<TMPro::TextMeshProUGUI>, titleText);
    DECLARE_INSTANCE_FIELD(UnityW<TMPro::TextMeshProUGUI>, statusText);
    DECLARE_INSTANCE_FIELD(UnityW<TMPro::TextMeshProUGUI>, artistText);
    DECLARE_INSTANCE_FIELD(UnityW<TMPro::TextMeshProUGUI>, mapperText);
    DECLARE_INSTANCE_FIELD(ArrayW<TMPro::TextMeshProUGUI*>, difficultyTexts);
    DECLARE_INSTANCE_FIELD(UnityW<UnityEngine::UI::HorizontalOrVerticalLayoutGroup>, difficultiesContainer);
    DECLARE_INSTANCE_FIELD(UnityW<HMUI::ImageView>, background);

  public:
    UpNextListCell* populate(const RecommendationItemState& item);
    void refreshBackground();
};
