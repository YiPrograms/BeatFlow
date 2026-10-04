#pragma once

#include "beatnext/quest/CompositionRoot.hpp"
#include "beatnext/quest/CustomTypeMacros.hpp"

#include "GlobalNamespace/PauseMenuManager.hpp"
#include "GlobalNamespace/ResultsViewController.hpp"
#include "HMUI/ModalView.hpp"
#include "HMUI/TableView.hpp"
#include "TMPro/TextMeshProUGUI.hpp"
#include "UnityEngine/MonoBehaviour.hpp"
#include "UnityEngine/UI/Button.hpp"
#include "UnityEngine/UI/Image.hpp"
#include "bsml/shared/BSML/Components/CustomListTableData.hpp"

DECLARE_CLASS_CODEGEN_INTERFACES(beatnext::quest, UpNextPanelController, UnityEngine::MonoBehaviour,
                                 HMUI::TableView::IDataSource*) {
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
    DECLARE_INSTANCE_FIELD(UnityW<BSML::CustomListTableData>, songList);
    DECLARE_INSTANCE_FIELD(bool, pauseContext);
    DECLARE_INSTANCE_FIELD(std::uint64_t, subscription);
    DECLARE_INSTANCE_FIELD(StringW, pendingHash);
    DECLARE_INSTANCE_FIELD(StringW, loadedArtworkUrl);
    DECLARE_INSTANCE_FIELD(UnityW<GlobalNamespace::PauseMenuManager>, pauseManager);
    DECLARE_INSTANCE_FIELD(UnityW<GlobalNamespace::ResultsViewController>, resultsView);

    DECLARE_INSTANCE_METHOD(void, SelectSong, UnityW<HMUI::TableView> table, int index);
    DECLARE_OVERRIDE_METHOD_MATCH(HMUI::TableCell*, CellForIdx, &HMUI::TableView::IDataSource::CellForIdx,
                                  HMUI::TableView * tableView, int index);
    DECLARE_OVERRIDE_METHOD_MATCH(float, CellSize, &HMUI::TableView::IDataSource::CellSize);
    DECLARE_OVERRIDE_METHOD_MATCH(int, NumberOfCells, &HMUI::TableView::IDataSource::NumberOfCells);
    DECLARE_INSTANCE_METHOD(void, Action);
    DECLARE_INSTANCE_METHOD(void, ConfirmExit);
    DECLARE_INSTANCE_METHOD(void, CancelExit);
    DECLARE_INSTANCE_METHOD(void, OnDestroy);

  public:
    void bind(bool pause, GlobalNamespace::PauseMenuManager* pauseManager,
              GlobalNamespace::ResultsViewController* resultsView);
    void render(const RecommendationSessionState& state);
};

namespace beatnext::quest::up_next_ui {
void showResults(GlobalNamespace::ResultsViewController* results);
void hideResults();
void showPause(GlobalNamespace::PauseMenuManager* pause);
void hidePause();
} // namespace beatnext::quest::up_next_ui
