#pragma once

#include "beatnext/quest/CompositionRoot.hpp"
#include "beatnext/quest/CustomTypeMacros.hpp"

#include "GlobalNamespace/PauseMenuManager.hpp"
#include "GlobalNamespace/ResultsViewController.hpp"
#include "HMUI/ModalView.hpp"
#include "TMPro/TextMeshProUGUI.hpp"
#include "UnityEngine/MonoBehaviour.hpp"
#include "UnityEngine/UI/Button.hpp"
#include "UnityEngine/UI/Image.hpp"

DECLARE_CLASS_CODEGEN(beatnext::quest, UpNextPanelController, UnityEngine::MonoBehaviour) {
    DECLARE_CTOR(ctor);
    DECLARE_INSTANCE_FIELD(UnityW<TMPro::TextMeshProUGUI>, headingText);
    DECLARE_INSTANCE_FIELD(UnityW<TMPro::TextMeshProUGUI>, statusText);
    DECLARE_INSTANCE_FIELD(UnityW<TMPro::TextMeshProUGUI>, detailTitleText);
    DECLARE_INSTANCE_FIELD(UnityW<TMPro::TextMeshProUGUI>, detailArtistText);
    DECLARE_INSTANCE_FIELD(UnityW<TMPro::TextMeshProUGUI>, detailMetaText);
    DECLARE_INSTANCE_FIELD(UnityW<TMPro::TextMeshProUGUI>, detailDifficultyText);
    DECLARE_INSTANCE_FIELD(UnityW<UnityEngine::UI::Image>, detailImage);
    DECLARE_INSTANCE_FIELD(UnityW<UnityEngine::UI::Button>, actionButton);
    DECLARE_INSTANCE_FIELD(UnityW<HMUI::ModalView>, confirmModal);
    DECLARE_INSTANCE_FIELD(bool, pauseContext);
    DECLARE_INSTANCE_FIELD(std::uint64_t, subscription);
    DECLARE_INSTANCE_FIELD(StringW, pendingHash);
    DECLARE_INSTANCE_FIELD(StringW, loadedArtworkUrl);
    DECLARE_INSTANCE_FIELD(UnityW<GlobalNamespace::PauseMenuManager>, pauseManager);
    DECLARE_INSTANCE_FIELD(UnityW<GlobalNamespace::ResultsViewController>, resultsView);
    DECLARE_INSTANCE_FIELD(UnityW<UnityEngine::UI::Button>, item0Button);
    DECLARE_INSTANCE_FIELD(UnityW<UnityEngine::UI::Button>, item1Button);
    DECLARE_INSTANCE_FIELD(UnityW<UnityEngine::UI::Button>, item2Button);
    DECLARE_INSTANCE_FIELD(UnityW<UnityEngine::UI::Button>, item3Button);
    DECLARE_INSTANCE_FIELD(UnityW<UnityEngine::UI::Button>, item4Button);
    DECLARE_INSTANCE_FIELD(UnityW<UnityEngine::UI::Button>, item5Button);
    DECLARE_INSTANCE_FIELD(UnityW<UnityEngine::UI::Button>, item6Button);
    DECLARE_INSTANCE_FIELD(UnityW<UnityEngine::UI::Button>, item7Button);
    DECLARE_INSTANCE_FIELD(UnityW<UnityEngine::UI::Button>, item8Button);
    DECLARE_INSTANCE_FIELD(UnityW<UnityEngine::UI::Button>, item9Button);
    DECLARE_INSTANCE_FIELD(UnityW<UnityEngine::UI::Button>, item10Button);
    DECLARE_INSTANCE_FIELD(UnityW<UnityEngine::UI::Button>, item11Button);
    DECLARE_INSTANCE_FIELD(UnityW<UnityEngine::UI::Button>, item12Button);
    DECLARE_INSTANCE_FIELD(UnityW<UnityEngine::UI::Button>, item13Button);
    DECLARE_INSTANCE_FIELD(UnityW<UnityEngine::UI::Button>, item14Button);
    DECLARE_INSTANCE_FIELD(UnityW<UnityEngine::UI::Button>, item15Button);
    DECLARE_INSTANCE_FIELD(UnityW<UnityEngine::UI::Button>, item16Button);
    DECLARE_INSTANCE_FIELD(UnityW<UnityEngine::UI::Button>, item17Button);
    DECLARE_INSTANCE_FIELD(UnityW<UnityEngine::UI::Button>, item18Button);
    DECLARE_INSTANCE_FIELD(UnityW<UnityEngine::UI::Button>, item19Button);
    DECLARE_INSTANCE_METHOD(void, Select0);
    DECLARE_INSTANCE_METHOD(void, Select1);
    DECLARE_INSTANCE_METHOD(void, Select2);
    DECLARE_INSTANCE_METHOD(void, Select3);
    DECLARE_INSTANCE_METHOD(void, Select4);
    DECLARE_INSTANCE_METHOD(void, Select5);
    DECLARE_INSTANCE_METHOD(void, Select6);
    DECLARE_INSTANCE_METHOD(void, Select7);
    DECLARE_INSTANCE_METHOD(void, Select8);
    DECLARE_INSTANCE_METHOD(void, Select9);
    DECLARE_INSTANCE_METHOD(void, Select10);
    DECLARE_INSTANCE_METHOD(void, Select11);
    DECLARE_INSTANCE_METHOD(void, Select12);
    DECLARE_INSTANCE_METHOD(void, Select13);
    DECLARE_INSTANCE_METHOD(void, Select14);
    DECLARE_INSTANCE_METHOD(void, Select15);
    DECLARE_INSTANCE_METHOD(void, Select16);
    DECLARE_INSTANCE_METHOD(void, Select17);
    DECLARE_INSTANCE_METHOD(void, Select18);
    DECLARE_INSTANCE_METHOD(void, Select19);
    DECLARE_INSTANCE_METHOD(void, Action);
    DECLARE_INSTANCE_METHOD(void, ConfirmExit);
    DECLARE_INSTANCE_METHOD(void, CancelExit);
    DECLARE_INSTANCE_METHOD(void, OnDestroy);

  public:
    void bind(bool pause, GlobalNamespace::PauseMenuManager* pauseManager,
              GlobalNamespace::ResultsViewController* resultsView);
    void render(const RecommendationSessionState& state);
    void select(std::size_t index);
};

namespace beatnext::quest::up_next_ui {
void showResults(GlobalNamespace::ResultsViewController* results);
void hideResults();
void showPause(GlobalNamespace::PauseMenuManager* pause);
void hidePause();
} // namespace beatnext::quest::up_next_ui
