#include "Test.hpp"

#include "beatflow/core/RecommendationEngine.hpp"

#include <map>

using namespace beatflow;

namespace {

class FakeMusicProvider final : public MusicProvider {
  public:
    std::vector<Track> homeTracks;
    std::vector<Track> radioTracks;
    std::vector<Track> searchTracks;
    std::map<std::string, std::vector<Track>> searchTracksByQuery;
    std::string lastRadioTrackId;
    std::string lastSearchQuery;
    std::vector<std::string> searchQueries;

    Outcome<std::vector<Track>> home(const CancellationToken&) override {
        return Outcome<std::vector<Track>>::success(homeTracks);
    }
    Outcome<std::vector<Track>> search(const std::string& query, const CancellationToken&) override {
        lastSearchQuery = query;
        searchQueries.push_back(query);
        if (const auto found = searchTracksByQuery.find(query); found != searchTracksByQuery.end()) {
            return Outcome<std::vector<Track>>::success(found->second);
        }
        return Outcome<std::vector<Track>>::success(searchTracks);
    }
    Outcome<std::vector<Track>> radio(const std::string& trackId, const CancellationToken&) override {
        lastRadioTrackId = trackId;
        return Outcome<std::vector<Track>>::success(radioTracks);
    }
};

class FakeMapCatalog final : public MapCatalog {
  public:
    std::map<std::string, std::vector<MapCandidate>> byTrack;
    Outcome<std::vector<MapCandidate>> search(const Track& track, const CancellationToken&) override {
        return Outcome<std::vector<MapCandidate>>::success(byTrack[track.providerId]);
    }
};

class FakeLibrary final : public SongLibrary {
  public:
    std::set<std::string> installed;
    bool isInstalled(const std::string& hash) const override {
        return installed.contains(hash);
    }
    Outcome<std::string> install(const MapCandidate& map, const CancellationToken&) override {
        installed.insert(map.hash);
        return Outcome<std::string>::success(map.hash);
    }
    Outcome<bool> openSongDetails(const std::string&) override {
        return Outcome<bool>::success(true);
    }
};

MapCandidate candidate(std::string hash, std::string title, std::string artist, double rating,
                       std::uint32_t votes = 100) {
    MapCandidate map;
    map.key = hash;
    map.hash = std::move(hash);
    map.songTitle = std::move(title);
    map.songArtist = std::move(artist);
    map.mapper = "Mapper";
    map.durationSeconds = 200;
    map.rating = rating;
    map.upvotes = votes;
    map.difficulties = {{Difficulty::Expert, "Standard", 5.0, {}}};
    return map;
}

} // namespace

BF_TEST("engine deduplicates tracks and maps while streaming accepted matches") {
    FakeMusicProvider music;
    FakeMapCatalog maps;
    FakeLibrary library;
    music.homeTracks = {{"one", "Song", {"Artist"}, 200, "", "Home", 0.9},
                        {"one", "Song", {"Artist"}, 200, "", "Home", 0.8},
                        {"two", "Second", {"Artist"}, 200, "", "Home", 0.7}};
    maps.byTrack["one"] = {candidate("hash-one", "Song", "Artist", 0.9)};
    maps.byTrack["two"] = {candidate("hash-one", "Second", "Artist", 0.99),
                           candidate("hash-two", "Second", "Artist", 0.8)};

    RecommendationEngine engine(music, maps, &library);
    CancellationSource cancellation;
    std::size_t progressCount = 0;
    const auto result =
        engine.forYou({}, cancellation.token(), [&progressCount](const RecommendedMap&) { ++progressCount; });
    BF_REQUIRE(result.ok());
    BF_REQUIRE(result.value().size() == 2);
    BF_REQUIRE(progressCount == 2);
}

BF_TEST("engine excludes current radio track and played hashes") {
    FakeMusicProvider music;
    FakeMapCatalog maps;
    FakeLibrary library;
    music.radioTracks = {{"current", "Current", {"Artist"}, 200, "", "Radio", 1.0},
                         {"next", "Next", {"Artist"}, 200, "", "Radio", 0.9}};
    maps.byTrack["next"] = {candidate("PLAYED", "Next", "Artist", 0.99),
                            candidate("fresh", "Next", "Artist", 0.9)};
    RecommendationRequest request;
    request.excludedMapHashes = {"played"};
    RecommendationEngine engine(music, maps, &library);
    CancellationSource cancellation;
    const auto result = engine.following("current", request, cancellation.token());
    BF_REQUIRE(result.ok());
    BF_REQUIRE(result.value().size() == 1);
    BF_REQUIRE(result.value().front().map.hash == "fresh");
}

BF_TEST("cancelled engine request stops before catalog work") {
    FakeMusicProvider music;
    FakeMapCatalog maps;
    music.homeTracks = {{"one", "Song", {"Artist"}, 200, "", "Home", 1.0}};
    RecommendationEngine engine(music, maps, nullptr);
    CancellationSource cancellation;
    cancellation.cancel();
    const auto result = engine.forYou({}, cancellation.token());
    BF_REQUIRE(!result.ok());
    BF_REQUIRE(result.error().code == ErrorCode::Cancelled);
}

BF_TEST("arbitrary Beat Saber metadata resolves to a confident YouTube track") {
    FakeMusicProvider music;
    FakeMapCatalog maps;
    music.searchTracks = {{"wrong", "Idol (Nightcore Remix)", {"YOASOBI"}, 175, "", "", 1.0},
                          {"right", "Idol", {"YOASOBI"}, 214, "", "", 1.0}};
    RecommendationEngine engine(music, maps, nullptr);
    CancellationSource cancellation;
    const auto result = engine.resolveTrack("Idol", "YOASOBI", 214, cancellation.token());
    BF_REQUIRE(result.ok());
    BF_REQUIRE(result.value().providerId == "right");
}

BF_TEST("short map edit can use the matching original recording as a radio seed") {
    FakeMusicProvider music;
    FakeMapCatalog maps;
    music.searchTracks = {{"wrong", "Time Left", {"Different Artist"}, 181, "", "", 1.0},
                          {"right", "残機 - Time Left", {"ZUTOMAYO"}, 181, "", "", 1.0}};
    RecommendationEngine engine(music, maps, nullptr);
    CancellationSource cancellation;

    const auto result = engine.resolveTrack("Time Left (TV Size)", "ZUTOMAYO", 91, cancellation.token());

    BF_REQUIRE(result.ok());
    BF_REQUIRE(result.value().providerId == "right");
    BF_REQUIRE(music.searchQueries.size() == 2);
    BF_REQUIRE(music.searchQueries.back() == "ZUTOMAYO Time Left");
}

BF_TEST("short map edit runs a broader search for the official full recording") {
    FakeMusicProvider music;
    FakeMapCatalog maps;
    music.searchTracksByQuery["ZUTOMAYO Time Left (TV Size)"] = {
        {"edit", "Time Left (TV Size)", {"Different Artist"}, 91, "", "", 1.0}};
    music.searchTracksByQuery["ZUTOMAYO Time Left"] = {
        {"official", "残機 - Time Left", {"ZUTOMAYO"}, 181, "", "", 1.0}};
    RecommendationEngine engine(music, maps, nullptr);
    CancellationSource cancellation;

    const auto result = engine.resolveTrack("Time Left (TV Size)", "ZUTOMAYO", 91, cancellation.token());

    BF_REQUIRE(result.ok());
    BF_REQUIRE(result.value().providerId == "official");
    BF_REQUIRE(music.searchQueries.size() == 2);
}

BF_TEST("radio seed uses the provider's best result when strict identity is unavailable") {
    FakeMusicProvider music;
    FakeMapCatalog maps;
    music.searchTracks = {{"best", "Official English Title", {"Official Artist"}, 210, "", "", 1.0},
                          {"second", "Unrelated Result", {"Other Artist"}, 180, "", "", 0.8}};
    RecommendationEngine engine(music, maps, nullptr);
    CancellationSource cancellation;

    const auto result =
        engine.resolveTrack("Localized Map Title", "Official Artist", 205, cancellation.token());

    BF_REQUIRE(result.ok());
    BF_REQUIRE(result.value().providerId == "best");
}

BF_TEST("Up Next resolves the current song then matches its radio recommendations") {
    FakeMusicProvider music;
    FakeMapCatalog maps;
    music.searchTracks = {{"current", "Current Song", {"Current Artist"}, 200, "", "", 1.0}};
    music.radioTracks = {{"current", "Current Song", {"Current Artist"}, 200, "", "Radio", 1.0},
                         {"next", "Next Song", {"Next Artist"}, 210, "", "Radio", 0.9}};
    maps.byTrack["next"] = {candidate("next-map", "Next Song", "Next Artist", 0.9)};
    RecommendationEngine engine(music, maps, nullptr);
    CancellationSource cancellation;

    const auto result = engine.upNext("Current Song", "Current Artist", 200, {}, cancellation.token());

    BF_REQUIRE(result.ok());
    BF_REQUIRE(result.value().size() == 1);
    BF_REQUIRE(result.value().front().track.providerId == "next");
    BF_REQUIRE(music.lastSearchQuery == "Current Artist Current Song");
    BF_REQUIRE(music.lastRadioTrackId == "current");
}
