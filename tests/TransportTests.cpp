#include "Test.hpp"

#include "beatflow/services/HttpHeaders.hpp"
#include "beatflow/services/RetryingHttpClient.hpp"
#include "beatflow/services/ZipArchiveValidator.hpp"

#include <queue>

using namespace beatflow;

namespace {

class SequenceHttp final : public HttpClient {
  public:
    std::queue<Outcome<HttpResponse>> responses;
    int calls{0};

    Outcome<HttpResponse> send(const HttpRequest&, const CancellationToken&) override {
        ++calls;
        auto response = std::move(responses.front());
        responses.pop();
        return response;
    }
};

void append16(std::vector<std::uint8_t>& bytes, std::uint16_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value));
    bytes.push_back(static_cast<std::uint8_t>(value >> 8U));
}

void append32(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
    append16(bytes, static_cast<std::uint16_t>(value));
    append16(bytes, static_cast<std::uint16_t>(value >> 16U));
}

std::vector<std::uint8_t> centralDirectoryOnly(const std::string& name) {
    std::vector<std::uint8_t> bytes;
    append32(bytes, 0x02014b50U);
    for (int index = 0; index < 6; ++index) {
        append16(bytes, 0);
    }
    append32(bytes, 0);
    append32(bytes, 4);
    append32(bytes, 4);
    append16(bytes, static_cast<std::uint16_t>(name.size()));
    append16(bytes, 0);
    append16(bytes, 0);
    append16(bytes, 0);
    append16(bytes, 0);
    append32(bytes, 0);
    append32(bytes, 0);
    bytes.insert(bytes.end(), name.begin(), name.end());
    const auto centralSize = static_cast<std::uint32_t>(bytes.size());
    append32(bytes, 0x06054b50U);
    append16(bytes, 0);
    append16(bytes, 0);
    append16(bytes, 1);
    append16(bytes, 1);
    append32(bytes, centralSize);
    append32(bytes, 0);
    append16(bytes, 0);
    return bytes;
}

} // namespace

BF_TEST("HTTP header parser recovers the final status from redirects") {
    constexpr std::string_view headers =
        "HTTP/1.1 200 Connection established\r\n\r\n"
        "HTTP/2 302\r\nlocation: https://example.test/final\r\n\r\n"
        "HTTP/2 200\r\ncontent-type: application/json\r\nretry-after: 2\r\n\r\n";

    BF_REQUIRE(http::statusFromRawHeaders(headers) == 200);
    const auto parsed = http::parseHeaders(headers);
    BF_REQUIRE(parsed.size() == 3);
    BF_REQUIRE(parsed.back().first == "retry-after");
    BF_REQUIRE(parsed.back().second == "2");
}

BF_TEST("HTTP retry honors a server delay and succeeds within its bound") {
    SequenceHttp inner;
    inner.responses.push(Outcome<HttpResponse>::success({429, {{"Retry-After", "2"}}, ""}));
    inner.responses.push(Outcome<HttpResponse>::success({200, {}, "ok"}));
    std::chrono::milliseconds observed{};
    RetryingHttpClient retrying(inner, {3, std::chrono::milliseconds(10), std::chrono::seconds(5)},
                                [&observed](auto delay, const auto&) {
                                    observed = delay;
                                    return true;
                                });
    CancellationSource cancellation;

    const auto result = retrying.send({}, cancellation.token());

    BF_REQUIRE(result.ok());
    BF_REQUIRE(result.value().body == "ok");
    BF_REQUIRE(inner.calls == 2);
    BF_REQUIRE(observed == std::chrono::seconds(2));
}

BF_TEST("HTTP retry stops during cancellable backoff") {
    SequenceHttp inner;
    inner.responses.push(Outcome<HttpResponse>::failure({ErrorCode::Network, "offline", true, std::nullopt}));
    CancellationSource cancellation;
    RetryingHttpClient retrying(inner, {}, [&cancellation](auto, const auto&) {
        cancellation.cancel();
        return false;
    });

    const auto result = retrying.send({}, cancellation.token());

    BF_REQUIRE(!result.ok());
    BF_REQUIRE(result.error().code == ErrorCode::Cancelled);
    BF_REQUIRE(inner.calls == 1);
}

BF_TEST("ZIP validation accepts map metadata and rejects traversal") {
    const auto valid = centralDirectoryOnly("Info.dat");
    const auto unsafe = centralDirectoryOnly("../Info.dat");

    BF_REQUIRE(ZipArchiveValidator::validate(valid).ok());
    const auto rejected = ZipArchiveValidator::validate(unsafe);
    BF_REQUIRE(!rejected.ok());
    BF_REQUIRE(rejected.error().code == ErrorCode::InvalidResponse);
}
