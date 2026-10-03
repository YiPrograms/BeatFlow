#include "beatflow/quest/QuestHttpClient.hpp"

#include "beatflow/services/HttpHeaders.hpp"

#include "web-utils/shared/WebUtils.hpp"

#include <span>

namespace beatflow::quest {
namespace {

ServiceError curlError(int status) {
    return {ErrorCode::Network, "The network request failed (curl " + std::to_string(status) + ").", true,
            std::nullopt};
}

} // namespace

Outcome<HttpResponse> QuestHttpClient::send(const HttpRequest& request,
                                            const CancellationToken& cancellation) {
    if (cancellation.isCancellationRequested()) {
        return Outcome<HttpResponse>::failure(
            {ErrorCode::Cancelled, "The network request was cancelled.", false, std::nullopt});
    }

    WebUtils::URLOptions::HeaderMap headers;
    for (const auto& [name, value] : request.headers) {
        headers.insert_or_assign(name, value);
    }
    WebUtils::URLOptions options(request.url, {}, std::move(headers), request.url.starts_with("https://"), "",
                                 std::nullopt, request.timeoutSeconds);
    options.noEscape = true;

    WebUtils::StringResponse response;
    if (request.method == HttpRequest::Method::Post) {
        const auto* bytes = reinterpret_cast<const std::uint8_t*>(request.body.data());
        response = WebUtils::Post<WebUtils::StringResponse>(
            std::move(options), std::span<const std::uint8_t>(bytes, request.body.size()));
    } else {
        response = WebUtils::Get<WebUtils::StringResponse>(std::move(options));
    }

    if (cancellation.isCancellationRequested()) {
        return Outcome<HttpResponse>::failure(
            {ErrorCode::Cancelled, "The network request was cancelled.", false, std::nullopt});
    }
    if (response.curlStatus != 0) {
        return Outcome<HttpResponse>::failure(curlError(response.curlStatus));
    }
    HttpResponse translated;
    translated.status = response.httpCode;
    if (translated.status == 0) {
        translated.status = http::statusFromRawHeaders(response.responseHeaders);
    }
    translated.headers = http::parseHeaders(response.responseHeaders);
    if (response.responseData) {
        translated.body = std::move(*response.responseData);
    }
    return Outcome<HttpResponse>::success(std::move(translated));
}

} // namespace beatflow::quest
