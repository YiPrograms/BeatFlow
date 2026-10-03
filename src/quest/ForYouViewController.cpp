#include "beatflow/quest/ForYouViewController.hpp"

#include "beatflow/quest/Assets.hpp"
#include "beatflow/quest/CompositionRoot.hpp"
#include "beatflow/quest/UI.hpp"

#include "UnityEngine/Component.hpp"
#include "beatsaber-hook/shared/utils/il2cpp-utils.hpp"
#include "bsml/shared/BSML.hpp"
#include "bsml/shared/BSML/MainThreadScheduler.hpp"
#include "bsml/shared/Helpers/utilities.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <sstream>

DEFINE_TYPE(beatflow::quest, ForYouViewController);

namespace beatflow::quest {
namespace {

constexpr std::size_t kCardsPerPage = 3;

void setText(TMPro::TextMeshProUGUI* target, const std::string& value) {
    if (target != nullptr) {
        target->set_text(il2cpp_utils::newcsstr(value));
    }
}

std::string difficulties(const RecommendedMap& recommendation) {
    std::ostringstream value;
    for (std::size_t index = 0; index < recommendation.playableDifficulties.size(); ++index) {
        if (index != 0) {
            value << " · ";
        }
        value << toString(recommendation.playableDifficulties[index].difficulty);
    }
    return value.str();
}

std::string card(const RecommendedMap& recommendation) {
    std::ostringstream value;
    value << "<b>" << recommendation.track.title << "</b>\n";
    if (!recommendation.track.artists.empty()) {
        value << recommendation.track.artists.front() << " · ";
    }
    value << "mapped by " << recommendation.map.mapper << "\n";
    value << static_cast<int>(std::round(recommendation.map.rating * 100.0)) << "% · "
          << difficulties(recommendation);
    value << (recommendation.installed ? " · Installed" : " · Download");
    return value.str();
}

} // namespace

void ForYouViewController::ctor() {
    INVOKE_CTOR();
    page = 0;
    difficultyMode = 0;
    npsMode = 0;
}

void ForYouViewController::DidActivate(bool firstActivation, bool, bool) {
    if (firstActivation) {
        BSML::parse_and_construct(Assets::ForYou_bsml, get_transform(), this);
    }
    page = 0;
    if (ui::openingRecommendedNext()) {
        render();
        if (CompositionRoot::instance().nextState().loading) {
            SafePtrUnity<ForYouViewController> view(this);
            BSML::MainThreadScheduler::ScheduleUntil(
                [] { return !CompositionRoot::instance().nextState().loading; },
                [view] {
                    if (view && ui::openingRecommendedNext()) {
                        view.ptr()->render();
                    }
                });
        }
    } else {
        render();
        const auto state = CompositionRoot::instance().browseState();
        if (state.recommendations.empty() && !state.loading) {
            setText(statusText,
                    "Personalized For You is optional. Add your OAuth client file, then select Connect. "
                    "Anonymous Up Next already works without an account.");
        }
    }
}

void ForYouViewController::DidDeactivate(bool, bool) {
    CompositionRoot::instance().cancelInteractive();
}

void ForYouViewController::Refresh() {
    page = 0;
    setText(statusText, "Finding personalized recommendations…");
    CompositionRoot::instance().refreshForYou(CompositionRoot::instance().filters(),
                                              [this](const RecommendationViewState&) { render(); });
}

void ForYouViewController::Connect() {
    CompositionRoot::instance().connect([this](const AuthorizationViewState& state) {
        std::string connection = state.message;
        if (!state.verificationUrl.empty()) {
            connection += "\n" + state.verificationUrl + "  Code: " + state.userCode;
        }
        setText(connectionText, connection);
        if (state.connected) {
            Refresh();
        }
    });
}

void ForYouViewController::Disconnect() {
    const auto result = CompositionRoot::instance().disconnect();
    setText(connectionText, result ? std::string("YouTube Music disconnected. Anonymous Up Next still works.")
                                   : result.error().message);
    render();
}

void ForYouViewController::ClearLocalData() {
    const auto result = CompositionRoot::instance().clearLocalData();
    setText(statusText, result ? std::string("BeatFlow's local account data and caches were cleared.")
                               : result.error().message);
    render();
}

void ForYouViewController::Cancel() {
    CompositionRoot::instance().cancelInteractive();
    setText(statusText, "Request cancelled.");
}

void ForYouViewController::PreviousPage() {
    page = std::max(0, page - 1);
    render();
}

void ForYouViewController::NextPage() {
    const auto state = CompositionRoot::instance().browseState();
    const auto pageCount =
        static_cast<int>((state.recommendations.size() + kCardsPerPage - 1) / kCardsPerPage);
    page = std::min(std::max(0, pageCount - 1), page + 1);
    render();
}

void ForYouViewController::CycleDifficulty() {
    difficultyMode = (difficultyMode + 1) % 4;
    auto filters = CompositionRoot::instance().filters();
    filters.difficulties.clear();
    if (difficultyMode == 1) {
        filters.difficulties.insert(Difficulty::Hard);
    } else if (difficultyMode == 2) {
        filters.difficulties.insert(Difficulty::Expert);
    } else if (difficultyMode == 3) {
        filters.difficulties.insert(Difficulty::ExpertPlus);
    }
    CompositionRoot::instance().setFilters(std::move(filters));
    Refresh();
}

void ForYouViewController::CycleNps() {
    npsMode = (npsMode + 1) % 4;
    auto filters = CompositionRoot::instance().filters();
    filters.minimumNps.reset();
    filters.maximumNps.reset();
    if (npsMode == 1) {
        filters.maximumNps = 4.0;
    } else if (npsMode == 2) {
        filters.minimumNps = 4.0;
        filters.maximumNps = 6.0;
    } else if (npsMode == 3) {
        filters.minimumNps = 6.0;
    }
    CompositionRoot::instance().setFilters(std::move(filters));
    Refresh();
}

bool ForYouViewController::get_showOnSongEnd() {
    return CompositionRoot::instance().showNextOnResults();
}

void ForYouViewController::set_showOnSongEnd(bool value) {
    CompositionRoot::instance().setShowNextOnResults(value);
}

bool ForYouViewController::get_showOnPause() {
    return CompositionRoot::instance().showNextOnPause();
}

void ForYouViewController::set_showOnPause(bool value) {
    CompositionRoot::instance().setShowNextOnPause(value);
}

void ForYouViewController::Select0() {
    select(0);
}
void ForYouViewController::Select1() {
    select(1);
}
void ForYouViewController::Select2() {
    select(2);
}

void ForYouViewController::select(std::size_t slot) {
    const auto state = CompositionRoot::instance().browseState();
    const auto index = static_cast<std::size_t>(page) * kCardsPerPage + slot;
    if (index >= state.recommendations.size()) {
        return;
    }
    setText(statusText, state.recommendations[index].installed ? "Opening song details…"
                                                               : "Downloading and validating map…");
    CompositionRoot::instance().prepare(
        state.recommendations[index], [this](Outcome<std::string> prepared) mutable {
            if (!prepared) {
                setText(statusText, prepared.error().message);
                return;
            }
            const auto hash = std::move(prepared).value();
            ui::close(true);
            BSML::MainThreadScheduler::Schedule([hash] {
                const auto opened = CompositionRoot::instance().openPrepared(hash);
                static_cast<void>(opened);
            });
        });
}

void ForYouViewController::render() {
    const auto state = CompositionRoot::instance().browseState();
    const auto filters = CompositionRoot::instance().filters();
    const auto pageCount =
        std::max<std::size_t>(1, (state.recommendations.size() + kCardsPerPage - 1) / kCardsPerPage);
    page = std::clamp(page, 0, static_cast<int>(pageCount - 1));

    if (state.loading) {
        setText(statusText, "Finding and matching maps… Results appear as they are ready.");
    } else if (state.error) {
        setText(statusText, state.error->message);
    } else if (state.recommendations.empty()) {
        const bool filtered = !filters.difficulties.empty() || filters.minimumNps || filters.maximumNps;
        setText(statusText, filtered ? "No compatible Standard maps passed the current filters."
                                     : "No confident BeatSaver matches were found. Try Refresh later.");
    } else {
        setText(statusText, state.context + " · " + std::to_string(state.recommendations.size()) + " maps" +
                                (state.stale ? " · Cached/offline" : ""));
    }

    const auto difficultyLabel = difficultyMode == 0   ? "All difficulties"
                                 : difficultyMode == 1 ? "Hard"
                                 : difficultyMode == 2 ? "Expert"
                                                       : "Expert+";
    const auto npsLabel = npsMode == 0   ? "Any NPS"
                          : npsMode == 1 ? "Up to 4 NPS"
                          : npsMode == 2 ? "4–6 NPS"
                                         : "6+ NPS";
    setText(filterText, std::string(difficultyLabel) + " · " + npsLabel + " · Standard");
    setText(pageText, "Page " + std::to_string(page + 1) + " / " + std::to_string(pageCount));

    const std::array<UnityEngine::UI::Button*, 3> buttons{card0Button, card1Button, card2Button};
    const std::array<UnityEngine::UI::Image*, 3> images{card0Image, card1Image, card2Image};
    for (std::size_t slot = 0; slot < kCardsPerPage; ++slot) {
        const auto index = static_cast<std::size_t>(page) * kCardsPerPage + slot;
        const bool visible = index < state.recommendations.size();
        if (buttons[slot] != nullptr) {
            buttons[slot]->get_gameObject()->set_active(visible);
            setText(buttons[slot]->GetComponentInChildren<TMPro::TextMeshProUGUI*>(),
                    visible ? card(state.recommendations[index]) : "");
        }
        if (images[slot] != nullptr) {
            images[slot]->get_gameObject()->set_active(visible);
            if (visible && !state.recommendations[index].map.coverUrl.empty()) {
                BSML::Utilities::SetImage(
                    images[slot], il2cpp_utils::newcsstr(state.recommendations[index].map.coverUrl), true);
            }
        }
    }
}

} // namespace beatflow::quest
