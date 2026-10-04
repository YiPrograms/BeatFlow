#include "beatnext/quest/SongSelectionNavigator.hpp"

#include "beatnext/quest/CompositionRoot.hpp"

#include "GlobalNamespace/BeatmapLevel.hpp"
#include "GlobalNamespace/BeatmapLevelPack.hpp"
#include "GlobalNamespace/LevelCollectionNavigationController.hpp"
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

    auto category = GlobalNamespace::SelectLevelCategoryViewController::LevelCategory::All;
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
    CompositionRoot::instance().rememberPlayed(hash);
    selectWhenReady(hash, 180, std::move(callback));
}

void SongSelectionNavigator::selectWhenReady(const std::string& hash, int attemptsRemaining,
                                             Callback callback) {
    auto* solo = BSML::Helpers::GetDiContainer()->Resolve<GlobalNamespace::SoloFreePlayFlowCoordinator*>();
    auto* level = SongCore::API::Loading::GetLevelByHash(hash);
    auto navigation = solo == nullptr ? nullptr : solo->___levelSelectionNavigationController;
    auto collection = navigation == nullptr ? nullptr : navigation->____levelCollectionNavigationController;
    if (solo != nullptr && solo->get_isActivated() && !solo->get_isInTransition() && level != nullptr &&
        collection != nullptr) {
        collection->SelectLevel(level);
        BSML::MainThreadScheduler::ScheduleNextFrame(
            [navigation, level, callback = std::move(callback)]() mutable {
                if (navigation != nullptr && navigation->get_beatmapLevel() == level) {
                    if (callback)
                        callback(Outcome<bool>::success(true));
                } else if (callback) {
                    callback(Outcome<bool>::failure(
                        {ErrorCode::Internal, "Solo opened, but Beat Saber did not select the requested map.",
                         true, std::nullopt}));
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
