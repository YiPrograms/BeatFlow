#include "beatflow/quest/QuestHttpClient.hpp"

#include "web-utils/shared/WebUtils.hpp"

#include <algorithm>
#include <cctype>
#include <span>
#include <sstream>

namespace beatflow::quest {
namespace {

std::vector<std::pair<std::string, std::string>> parseHeaders(const std::string& raw) {
    std::vector<std::pair<std::string, std::string>> result;
    std::istringstream lines(raw);
    for (std::string line; std::getline(lines, line);) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        const auto separator = line.find(':');
        if (separator == std::string::npos) {
            continue;
        }
        auto name = line.substr(0, separator);
        auto value = line.substr(separator + 1);
        value.erase(value.begin(), std::find_if(value.begin(), value.end(), [](unsigned char character) {
                        return std::isspace(character) == 0;
                    }));
        result.emplace_back(std::move(name), std::move(value));
    }
    return result;
}

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
    translated.headers = parseHeaders(response.responseHeaders);
    if (response.responseData) {
        translated.body = std::move(*response.responseData);
    }
    return Outcome<HttpResponse>::success(std::move(translated));
}

} // namespace beatflow::quest
