#include "beatnext/quest/UpNextListCell.hpp"

#include "UnityEngine/Color.hpp"
#include "UnityEngine/GameObject.hpp"
#include "beatsaber-hook/shared/utils/il2cpp-utils.hpp"

#include <cmath>
#include <sstream>

DEFINE_TYPE(beatnext::quest, UpNextListCell);

namespace beatnext::quest {
namespace {

void setText(TMPro::TextMeshProUGUI* target, const std::string& value) {
    if (target != nullptr)
        target->set_text(il2cpp_utils::newcsstr(value));
}

std::string artists(const Track& track) {
    if (track.artists.empty())
        return "Unknown artist";
    std::ostringstream value;
    for (std::size_t index = 0; index < track.artists.size(); ++index) {
        if (index != 0)
            value << " · ";
        value << track.artists[index];
    }
    return value.str();
}

std::string difficultyLabel(Difficulty difficulty) {
    switch (difficulty) {
    case Difficulty::Easy:
        return "<color=#78D58A>Easy</color>";
    case Difficulty::Normal:
        return "<color=#69C9F0>Normal</color>";
    case Difficulty::Hard:
        return "<color=#F3C85B>Hard</color>";
    case Difficulty::Expert:
        return "<color=#F07A76>Expert</color>";
    case Difficulty::ExpertPlus:
        return "<color=#C68AF4>Expert+</color>";
    }
    return {};
}

bool isInstalled(const RecommendationItemState& item) {
    return item.recommendation.installed || item.status == RecommendationItemStatus::Installed;
}

} // namespace

void UpNextListCell::ctor() {
    INVOKE_BASE_CTOR(classof(HMUI::TableCell*));
}

UpNextListCell* UpNextListCell::populate(const RecommendationItemState& item) {
    const auto& recommendation = item.recommendation;
    setText(titleText, recommendation.track.title);
    setText(artistText, artists(recommendation.track));

    std::ostringstream mapper;
    mapper << "Mapped by " << recommendation.map.mapper << " · "
           << static_cast<int>(std::round(recommendation.map.rating * 100.0)) << "%";
    setText(mapperText, mapper.str());

    if (item.status == RecommendationItemStatus::Downloading)
        setText(statusText, "<color=#69C9F0>Downloading…</color>");
    else if (item.status == RecommendationItemStatus::Failed)
        setText(statusText, "<color=#F07A76>Failed</color>");
    else if (isInstalled(item))
        setText(statusText, "<color=#78D58A>Downloaded</color>");
    else
        setText(statusText, "");

    for (int index = 0; index < difficultyTexts.size(); ++index) {
        auto* text = difficultyTexts[index];
        const bool visible = static_cast<std::size_t>(index) < recommendation.playableDifficulties.size();
        text->get_gameObject()->set_active(visible);
        if (visible)
            setText(text,
                    difficultyLabel(
                        recommendation.playableDifficulties[static_cast<std::size_t>(index)].difficulty));
    }
    refreshBackground();
    return this;
}

void UpNextListCell::refreshBackground() {
    if (background != nullptr)
        background->set_color(
            UnityEngine::Color(0.0F, 0.0F, 0.0F, (get_selected() || get_highlighted()) ? 0.82F : 0.45F));
}

void UpNextListCell::SelectionDidChange(HMUI::SelectableCell::TransitionType) {
    refreshBackground();
}

void UpNextListCell::HighlightDidChange(HMUI::SelectableCell::TransitionType) {
    refreshBackground();
}

void UpNextListCell::WasPreparedForReuse() {}

} // namespace beatnext::quest
