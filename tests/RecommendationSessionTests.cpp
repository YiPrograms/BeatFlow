#include "Test.hpp"

#include "beatnext/core/RecommendationSession.hpp"

using namespace beatnext;

namespace {
RecommendedMap recommendation(std::string hash, bool installed = false) {
    RecommendedMap value;
    value.track = {"video-" + hash, "完全な曲名 " + hash, {"Artist"}, 200, "", "Radio", 1.0};
    value.map.hash = std::move(hash);
    value.installed = installed;
    return value;
}
} // namespace

BF_TEST("recommendation session discards stale generations") {
    RecommendationSession session;
    const auto first = session.begin("First");
    const auto second = session.begin("Second");
    BF_REQUIRE(!session.finish(first, {recommendation("old")}));
    BF_REQUIRE(session.finish(second, {recommendation("new")}));
    const auto state = session.state();
    BF_REQUIRE(state.items.size() == 1);
    BF_REQUIRE(state.items.front().recommendation.map.hash == "new");
}

BF_TEST("recommendation session publishes selection and download states") {
    RecommendationSession session;
    int updates = 0;
    const auto token = session.subscribe([&updates](const RecommendationSessionState&) { ++updates; });
    const auto generation = session.begin("After song");
    BF_REQUIRE(session.finish(generation, {recommendation("one"), recommendation("two", true)}));
    BF_REQUIRE(session.select(1));
    BF_REQUIRE(session.updateItem(generation, 0, RecommendationItemStatus::Downloading, "Downloading"));
    BF_REQUIRE(session.updateItem(generation, 0, RecommendationItemStatus::Installed, "Installed", true));
    const auto state = session.state();
    BF_REQUIRE(state.selectedIndex == 1);
    BF_REQUIRE(state.items[0].recommendation.installed);
    BF_REQUIRE(state.items[0].status == RecommendationItemStatus::Installed);
    BF_REQUIRE(updates == 6);
    session.unsubscribe(token);
}
