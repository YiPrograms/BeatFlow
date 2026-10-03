#include "beatflow/quest/RecommendedNextPanel.hpp"

#include "beatflow/quest/Assets.hpp"
#include "beatflow/quest/CompositionRoot.hpp"
#include "beatflow/quest/UI.hpp"

#include "UnityEngine/Component.hpp"
#include "beatsaber-hook/shared/utils/il2cpp-utils.hpp"
#include "bsml/shared/BSML.hpp"
#include "bsml/shared/BSML/MainThreadScheduler.hpp"
#include "bsml/shared/Helpers/getters.hpp"
#include "bsml/shared/Helpers/utilities.hpp"

#include <array>
#include <cmath>
#include <functional>

DEFINE_TYPE(beatflow::quest, RecommendedNextPanel);

namespace beatflow::quest {
namespace {

void setText(TMPro::TextMeshProUGUI* target, const std::string& value) {
    if (target != nullptr) {
        target->set_text(il2cpp_utils::newcsstr(value));
    }
}

std::string label(const RecommendedMap& recommendation) {
    std::string artist =
        recommendation.track.artists.empty() ? "Unknown artist" : recommendation.track.artists.front();
    return "<b>" + recommendation.track.title + "</b>\n" + artist + " · " + recommendation.map.mapper +
           " · " + std::to_string(static_cast<int>(std::round(recommendation.map.rating * 100.0))) + "%";
}

void afterResultsClose(GlobalNamespace::ResultsViewController* results, std::function<void()> action) {
    SafePtrUnity<GlobalNamespace::ResultsViewController> closingResults(results);
    if (results != nullptr) {
        results->ContinueButtonPressed();
    }
    BSML::MainThreadScheduler::ScheduleNextFrame([closingResults, action = std::move(action)]() mutable {
        BSML::MainThreadScheduler::ScheduleUntil(
            [closingResults] {
                auto current = BSML::Helpers::GetMainFlowCoordinator()->YoungestChildFlowCoordinatorOrSelf();
                const bool resultsClosed = !closingResults || !closingResults.ptr()->get_isActiveAndEnabled();
                return resultsClosed && current != nullptr && current->get_isActivated() &&
                       !current->get_isInTransition();
            },
            std::move(action));
    });
}

} // namespace

void RecommendedNextPanel::ctor() {
    INVOKE_CTOR();
}

void RecommendedNextPanel::bind(GlobalNamespace::ResultsViewController* results) {
    resultsView = results;
    if (headingText == nullptr) {
        BSML::parse_and_construct(Assets::RecommendedNext_bsml, get_transform(), this);
    }
    render();
    if (CompositionRoot::instance().nextState().loading) {
        SafePtrUnity<RecommendedNextPanel> panel(this);
        BSML::MainThreadScheduler::ScheduleUntil(
            [] { return !CompositionRoot::instance().nextState().loading; },
            [panel] {
                if (panel) {
                    panel.ptr()->render();
                }
            });
    }
}

void RecommendedNextPanel::render() {
    const auto state = CompositionRoot::instance().nextState();
    setText(headingText, state.loading ? "Up Next · still finding maps…"
                         : state.stale ? "Up Next · Cached/offline"
                                       : "Up Next");
    if (state.error) {
        setText(headingText, "Up Next unavailable\n" + state.error->message);
    } else if (state.recommendations.empty() && !state.loading) {
        setText(headingText, "Up Next · no confident map matches");
    }
    if (seeMoreButton != nullptr) {
        setText(seeMoreButton->GetComponentInChildren<TMPro::TextMeshProUGUI*>(),
                state.error ? "Open For You" : "See more");
    }

    const std::array<UnityEngine::UI::Button*, 3> buttons{item0Button, item1Button, item2Button};
    const std::array<UnityEngine::UI::Image*, 3> images{item0Image, item1Image, item2Image};
    for (std::size_t index = 0; index < buttons.size(); ++index) {
        const bool visible = index < state.recommendations.size();
        if (buttons[index] != nullptr) {
            buttons[index]->get_gameObject()->set_active(visible);
            setText(buttons[index]->GetComponentInChildren<TMPro::TextMeshProUGUI*>(),
                    visible ? label(state.recommendations[index]) : "");
        }
        if (images[index] != nullptr) {
            images[index]->get_gameObject()->set_active(visible);
            if (visible && !state.recommendations[index].map.coverUrl.empty()) {
                BSML::Utilities::SetImage(
                    images[index], il2cpp_utils::newcsstr(state.recommendations[index].map.coverUrl), true);
            }
        }
    }
}

void RecommendedNextPanel::Select0() {
    select(0);
}
void RecommendedNextPanel::Select1() {
    select(1);
}
void RecommendedNextPanel::Select2() {
    select(2);
}

void RecommendedNextPanel::select(std::size_t index) {
    const auto state = CompositionRoot::instance().nextState();
    if (index >= state.recommendations.size()) {
        return;
    }
    setText(headingText,
            state.recommendations[index].installed ? "Opening Up Next…" : "Downloading Up Next…");
    CompositionRoot::instance().prepare(
        state.recommendations[index], [this](Outcome<std::string> prepared) mutable {
            if (!prepared) {
                setText(headingText, prepared.error().message);
                return;
            }
            const auto hash = std::move(prepared).value();
            afterResultsClose(resultsView, [hash] {
                const auto opened = CompositionRoot::instance().openPrepared(hash);
                static_cast<void>(opened);
            });
        });
}

void RecommendedNextPanel::SeeMore() {
    if (CompositionRoot::instance().nextState().error) {
        CompositionRoot::instance().browseForYouRecommendations();
        afterResultsClose(resultsView, [] { ui::showForYou(); });
    } else {
        CompositionRoot::instance().browseNextRecommendations();
        afterResultsClose(resultsView, [] { ui::showRecommendedNext(); });
    }
}

} // namespace beatflow::quest
