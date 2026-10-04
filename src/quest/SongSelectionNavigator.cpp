#include "beatnext/quest/SongSelectionNavigator.hpp"

#include "beatnext/quest/CompositionRoot.hpp"

#include "GlobalNamespace/BeatmapLevel.hpp"
#include "GlobalNamespace/BeatmapLevelPack.hpp"
#include "GlobalNamespace/LevelCollectionNavigationController.hpp"
#include "GlobalNamespace/LevelCollectionTableView.hpp"
#include "GlobalNamespace/LevelCollectionViewController.hpp"
#include "GlobalNamespace/LevelSelectionFlowCoordinator.hpp"
#include "GlobalNamespace/LevelSelectionNavigationController.hpp"
#include "GlobalNamespace/SelectLevelCategoryViewController.hpp"
#include "GlobalNamespace/SoloFreePlayFlowCoordinator.hpp"
#include "HMUI/NoTransitionsButton.hpp"
#include "System/Nullable_1.hpp"
#include "UnityEngine/GameObject.hpp"
#include "bsml/shared/BSML/MainThreadScheduler.hpp"
#include "bsml/shared/Helpers/getters.hpp"
#include "songcore/shared/SongCore.hpp"

#include <algorithm>
#include <cctype>

namespace {

std::string normalizedHash(std::string value) {
    constexpr std::string_view prefix = "custom_level_";
    if (value.starts_with(prefix))
        value.erase(0, prefix.size());
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return value;
}

bool isSelected(GlobalNamespace::LevelSelectionNavigationController* navigation,
                GlobalNamespace::LevelCollectionTableView* table, const std::string& hash) {
    if (navigation == nullptr || table == nullptr || navigation->get_beatmapLevel() == nullptr ||
        table->____selectedBeatmapLevel == nullptr)
        return false;
    const auto target = normalizedHash(hash);
    return normalizedHash(static_cast<std::string>(navigation->get_beatmapLevel()->___levelID)) == target &&
           normalizedHash(static_cast<std::string>(table->____selectedBeatmapLevel->___levelID)) == target;
}

} // namespace

namespace beatnext::quest {

void SongSelectionNavigator::open(const std::string& hash, Callback callback) {
    auto* level = SongCore::API::Loading::GetLevelByHash(hash);
    auto* pack = SongCore::API::Loading::GetCustomLevelPack();
    auto* solo = BSML::Helpers::GetDiContainer()->Resolve<GlobalNamespace::SoloFreePlayFlowCoordinator*>();
    if (level == nullptr || pack == nullptr || solo == nullptr) {
        if (callback)
            callback(Outcome<bool>::failure(
                {ErrorCode::NotFound, "The downloaded map is not available in SongCore's custom song pack.",
                 true, std::nullopt}));
        return;
    }

    auto category = GlobalNamespace::SelectLevelCategoryViewController::LevelCategory::CustomSongs;
    System::Nullable_1<GlobalNamespace::SelectLevelCategoryViewController::LevelCategory> nullableCategory;
    nullableCategory.value = category;
    nullableCategory.hasValue = true;
    auto* state = GlobalNamespace::LevelSelectionFlowCoordinator::State::New_ctor(
        static_cast<GlobalNamespace::BeatmapLevelPack*>(pack),
        static_cast<GlobalNamespace::BeatmapLevel*>(level));
    state->___levelCategory = nullableCategory;
    solo->Setup(state);

    auto buttonObject = UnityEngine::GameObject::Find("SoloButton");
    if (buttonObject == nullptr) {
        buttonObject =
            UnityEngine::GameObject::Find("Wrapper/BeatmapWithModifiers/BeatmapSelection/EditButton");
    }
    auto* button =
        buttonObject == nullptr ? nullptr : buttonObject->GetComponent<HMUI::NoTransitionsButton*>();
    if (button == nullptr) {
        if (callback)
            callback(Outcome<bool>::failure(
                {ErrorCode::Internal, "Beat Saber's Solo button is unavailable.", true, std::nullopt}));
        return;
    }
    button->Press();
    selectWhenReady(hash, 240, std::move(callback));
}

void SongSelectionNavigator::selectWhenReady(const std::string& hash, int attemptsRemaining,
                                             Callback callback) {
    auto* solo = BSML::Helpers::GetDiContainer()->Resolve<GlobalNamespace::SoloFreePlayFlowCoordinator*>();
    auto* level = SongCore::API::Loading::GetLevelByHash(hash);
    auto navigation = solo == nullptr ? nullptr : solo->___levelSelectionNavigationController;
    auto collection = navigation == nullptr ? nullptr : navigation->____levelCollectionNavigationController;
    auto collectionView = collection == nullptr ? nullptr : collection->____levelCollectionViewController;
    auto table = collectionView == nullptr ? nullptr : collectionView->____levelCollectionTableView;
    if (solo != nullptr && solo->get_isActivated() && !solo->get_isInTransition() && level != nullptr &&
        collection != nullptr && collectionView != nullptr && table != nullptr) {
        collection->____beatmapLevelToBeSelectedAfterPresent = level;
        collectionView->____beatmapLevelToBeSelected = level;
        collection->SelectLevel(level);
        table->SelectLevel(level);
        BSML::MainThreadScheduler::ScheduleNextFrame([this, hash, attemptsRemaining,
                                                      callback = std::move(callback)]() mutable {
            auto* currentSolo =
                BSML::Helpers::GetDiContainer()->Resolve<GlobalNamespace::SoloFreePlayFlowCoordinator*>();
            auto currentNavigation =
                currentSolo == nullptr ? nullptr : currentSolo->___levelSelectionNavigationController;
            auto currentCollection = currentNavigation == nullptr
                                         ? nullptr
                                         : currentNavigation->____levelCollectionNavigationController;
            auto currentView =
                currentCollection == nullptr ? nullptr : currentCollection->____levelCollectionViewController;
            auto currentTable = currentView == nullptr ? nullptr : currentView->____levelCollectionTableView;
            if (isSelected(currentNavigation, currentTable, hash)) {
                CompositionRoot::instance().rememberPlayed(hash);
                if (callback)
                    callback(Outcome<bool>::success(true));
            } else if (attemptsRemaining > 0) {
                selectWhenReady(hash, attemptsRemaining - 1, std::move(callback));
            } else if (callback) {
                callback(Outcome<bool>::failure(
                    {ErrorCode::Internal,
                     "Solo opened, but the requested map was not selected in the visible song list.", true,
                     std::nullopt}));
            }
        });
        return;
    }
    if (attemptsRemaining <= 0) {
        if (callback)
            callback(Outcome<bool>::failure({ErrorCode::Internal,
                                             "Beat Saber's Solo song picker did not become ready.", true,
                                             std::nullopt}));
        return;
    }
    BSML::MainThreadScheduler::ScheduleNextFrame(
        [this, hash, attemptsRemaining, callback = std::move(callback)]() mutable {
            selectWhenReady(hash, attemptsRemaining - 1, std::move(callback));
        });
}

} // namespace beatnext::quest
