#include "Test.hpp"

#include "beatnext/core/Matcher.hpp"
#include "beatnext/core/TextNormalizer.hpp"

using namespace beatnext;

namespace {

MapCandidate map(std::string title, std::string artist, int duration = 210) {
    MapCandidate result;
    result.key = "abc";
    result.hash = "012345";
    result.songTitle = std::move(title);
    result.songArtist = std::move(artist);
    result.mapper = "Mapper";
    result.durationSeconds = duration;
    result.rating = 0.95;
    result.upvotes = 200;
    result.downvotes = 5;
    result.difficulties = {{Difficulty::Expert, "Standard", 5.1, {}},
                           {Difficulty::ExpertPlus, "Standard", 6.4, {}}};
    return result;
}

Track track(std::string title, std::string artist, int duration = 210) {
    return {"video", std::move(title), {std::move(artist)}, duration, "", "Home", 0.9};
}

} // namespace

BF_TEST("normalizer preserves Unicode and removes presentation noise") {
    TextNormalizer normalizer;
    const auto value = normalizer.title("【Official Music Video】唱 - Ado");
    BF_REQUIRE(value.text.find("唱") != std::string::npos);
    BF_REQUIRE(value.text.find("official") == std::string::npos);
    BF_REQUIRE(value.text.find("video") == std::string::npos);
}

BF_TEST("exact multilingual track matches the corresponding map") {
    Matcher matcher;
    auto result = matcher.evaluate(track("アイドル", "YOASOBI", 214), map("アイドル", "YOASOBI", 214));
    BF_REQUIRE(result.has_value());
    BF_REQUIRE(result->scores.identity > 0.95);
}

BF_TEST("bilingual provider titles match an English BeatSaver title alias") {
    Matcher matcher;
    auto result =
        matcher.evaluate(track("残機 - Time Left", "ZUTOMAYO", 232), map("Time Left", "ZUTOMAYO", 232));
    BF_REQUIRE(result.has_value());
    BF_REQUIRE(result->scores.title > 0.95);
}

BF_TEST("recording markers prevent a popular remix from replacing an exact song") {
    Matcher matcher;
    auto result =
        matcher.evaluate(track("Idol", "YOASOBI", 214), map("Idol (Nightcore Remix)", "YOASOBI", 175));
    BF_REQUIRE(!result.has_value());
}

BF_TEST("matcher retains compatible Standard difficulties only") {
    Matcher matcher;
    auto candidate = map("Idol", "YOASOBI", 214);
    candidate.difficulties.push_back({Difficulty::Hard, "Standard", 3.8, {"Noodle Extensions"}});
    candidate.difficulties.push_back({Difficulty::Hard, "OneSaber", 3.7, {}});
    const auto playable = matcher.playableDifficulties(candidate);
    BF_REQUIRE(playable.size() == 2);
    BF_REQUIRE(playable.front().difficulty == Difficulty::Expert);
    BF_REQUIRE(playable.back().difficulty == Difficulty::ExpertPlus);
}

BF_TEST("missing duration is neutral rather than a false rejection") {
    Matcher matcher;
    auto source = track("Cheerleader", "Porter Robinson");
    source.durationSeconds.reset();
    auto candidate = map("Cheerleader", "Porter Robinson");
    candidate.durationSeconds.reset();
    auto result = matcher.evaluate(source, candidate);
    BF_REQUIRE(result.has_value());
    BF_REQUIRE_NEAR(result->scores.duration, 0.65, 0.001);
}
