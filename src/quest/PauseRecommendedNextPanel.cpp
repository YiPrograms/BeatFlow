#include "beatflow/quest/PauseRecommendedNextPanel.hpp"

#include "beatflow/quest/Assets.hpp"
#include "beatflow/quest/CompositionRoot.hpp"

#include "beatsaber-hook/shared/utils/il2cpp-utils.hpp"
#include "bsml/shared/BSML.hpp"
#include "bsml/shared/BSML/MainThreadScheduler.hpp"

#include <array>

DEFINE_TYPE(beatflow::quest, PauseRecommendedNextPanel);

namespace beatflow::quest {
namespace {

void setText(TMPro::TextMeshProUGUI* target, const std::string& value) {
    if (target != nullptr) {
        target->set_text(il2cpp_utils::newcsstr(value));
    }
}

std::string label(const RecommendedMap& recommendation) {
    const auto artist =
        recommendation.track.artists.empty() ? "Unknown artist" : recommendation.track.artists.front();
    return "<b>" + recommendation.track.title + "</b>\n" + artist + " · " + recommendation.map.mapper;
}

} // namespace

void PauseRecommendedNextPanel::ctor() {
    INVOKE_CTOR();
}

void PauseRecommendedNextPanel::bind() {
    BSML::parse_and_construct(Assets::PauseRecommendedNext_bsml, get_transform(), this);
    render();
    if (CompositionRoot::instance().nextState().loading) {
        SafePtrUnity<PauseRecommendedNextPanel> panel(this);
        BSML::MainThreadScheduler::ScheduleUntil(
            [] { return !CompositionRoot::instance().nextState().loading; },
            [panel] {
                if (panel) {
                    panel.ptr()->render();
                }
            });
    }
}

void PauseRecommendedNextPanel::render() {
    const auto state = CompositionRoot::instance().nextState();
    if (state.loading) {
        setText(headingText, "Up Next · finding maps…");
    } else if (state.error) {
        setText(headingText, "Up Next unavailable\n" + state.error->message);
    } else if (state.recommendations.empty()) {
        setText(headingText, "Up Next · no confident map matches");
    } else {
        setText(headingText, state.stale ? "Up Next · Cached/offline" : "Up Next");
    }

    const std::array<TMPro::TextMeshProUGUI*, 3> labels{item0Text, item1Text, item2Text};
    for (std::size_t index = 0; index < labels.size(); ++index) {
        const bool visible = index < state.recommendations.size();
        if (labels[index] != nullptr) {
            labels[index]->get_gameObject()->set_active(visible);
            setText(labels[index], visible ? label(state.recommendations[index]) : "");
        }
    }
}

} // namespace beatflow::quest
