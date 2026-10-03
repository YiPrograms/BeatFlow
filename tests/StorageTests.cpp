#include "Test.hpp"

#include "beatflow/services/AtomicJsonCache.hpp"
#include "beatflow/services/WorkerQueue.hpp"
#include "beatflow/services/ZipArchiveExtractor.hpp"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <iterator>

using namespace beatflow;

BF_TEST("atomic cache rejects unsafe keys and supports overwrite and clear") {
    const auto root = std::filesystem::temp_directory_path() /
                      ("beatflow-cache-test-" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    AtomicJsonCache cache(root, 1024);
    BF_REQUIRE(!cache.write("../escape", "bad").ok());
    BF_REQUIRE(cache.write("valid_key", R"({"value":1})").ok());
    BF_REQUIRE(cache.write("valid_key", R"({"value":2})").ok());
    const auto read = cache.read("valid_key");
    BF_REQUIRE(read.ok());
    BF_REQUIRE(read.value() == R"({"value":2})");
    BF_REQUIRE(cache.clear().ok());
    BF_REQUIRE(!cache.read("valid_key").ok());
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
}

BF_TEST("atomic cache prunes old entries at its configured bound") {
    const auto root = std::filesystem::temp_directory_path() /
                      ("beatflow-cache-bound-test-" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    AtomicJsonCache cache(root, 1024, 2);
    BF_REQUIRE(cache.write("first", "1").ok());
    BF_REQUIRE(cache.write("second", "2").ok());
    BF_REQUIRE(cache.write("third", "3").ok());

    std::size_t entries = 0;
    for (const auto& entry : std::filesystem::directory_iterator(root)) {
        entries += entry.is_regular_file() && entry.path().extension() == ".json" ? 1 : 0;
    }
    BF_REQUIRE(entries == 2);
    BF_REQUIRE(cache.read("third").ok());

    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
}

BF_TEST("bounded worker queue executes accepted work") {
    WorkerQueue queue(1, 2);
    std::promise<void> completed;
    auto future = completed.get_future();
    BF_REQUIRE(queue.submit([&completed] { completed.set_value(); }));
    BF_REQUIRE(future.wait_for(std::chrono::seconds(2)) == std::future_status::ready);
    queue.stop();
    BF_REQUIRE(!queue.submit([] {}));
}

BF_TEST("validated map archive extracts the exact downloaded bytes") {
    const auto fixture = std::filesystem::path(BEATFLOW_FIXTURE_DIR) / "map_archive.zip";
    std::ifstream input(fixture, std::ios::binary);
    const std::vector<char> bytes(std::istreambuf_iterator<char>(input), {});
    const auto root = std::filesystem::temp_directory_path() /
                      ("beatflow-extract-test-" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto* data = reinterpret_cast<const std::uint8_t*>(bytes.data());

    const auto extracted = ZipArchiveExtractor::extract({data, bytes.size()}, root);

    BF_REQUIRE(extracted.ok());
    BF_REQUIRE(std::filesystem::exists(root / "Info.dat"));
    BF_REQUIRE(std::filesystem::file_size(root / "ExpertPlusStandard.dat") > 0);
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
}
