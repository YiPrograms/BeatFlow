#include "beatflow/quest/QuestHttpClient.hpp"

#include "beatflow/services/HttpHeaders.hpp"

#include "libcurl/shared/curl.h"

#include <array>
#include <limits>
#include <string>

namespace beatflow::quest {
namespace {

class CurlHandle {
  public:
    CurlHandle() : value_(curl_easy_init()) {}
    ~CurlHandle() {
        if (value_ != nullptr) {
            curl_easy_cleanup(value_);
        }
    }
    CurlHandle(const CurlHandle&) = delete;
    CurlHandle& operator=(const CurlHandle&) = delete;
    [[nodiscard]] CURL* get() const {
        return value_;
    }

  private:
    CURL* value_;
};

class CurlHeaders {
  public:
    ~CurlHeaders() {
        if (value_ != nullptr) {
            curl_slist_free_all(value_);
        }
    }
    bool append(const std::string& value) {
        auto* updated = curl_slist_append(value_, value.c_str());
        if (updated == nullptr) {
            return false;
        }
        value_ = updated;
        return true;
    }
    [[nodiscard]] curl_slist* get() const {
        return value_;
    }

  private:
    curl_slist* value_{nullptr};
};

CURLcode initializeCurl() {
    static const auto status = curl_global_init(CURL_GLOBAL_DEFAULT);
    return status;
}

std::size_t appendBytes(char* bytes, std::size_t size, std::size_t count, void* destination) {
    const auto byteCount = size * count;
    static_cast<std::string*>(destination)->append(bytes, byteCount);
    return byteCount;
}

int cancelTransfer(void* context, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
    return static_cast<const CancellationToken*>(context)->isCancellationRequested() ? 1 : 0;
}

ServiceError curlError(CURLcode status, const std::array<char, CURL_ERROR_SIZE>& details) {
    std::string message = "The network request failed: ";
    message += details.front() == '\0' ? curl_easy_strerror(status) : details.data();
    return {ErrorCode::Network, std::move(message), true, std::nullopt};
}

} // namespace

Outcome<HttpResponse> QuestHttpClient::send(const HttpRequest& request,
                                            const CancellationToken& cancellation) {
    if (cancellation.isCancellationRequested()) {
        return Outcome<HttpResponse>::failure(
            {ErrorCode::Cancelled, "The network request was cancelled.", false, std::nullopt});
    }

    const auto initializationStatus = initializeCurl();
    if (initializationStatus != CURLE_OK) {
        std::array<char, CURL_ERROR_SIZE> emptyDetails{};
        return Outcome<HttpResponse>::failure(curlError(initializationStatus, emptyDetails));
    }
    CurlHandle curl;
    if (curl.get() == nullptr) {
        return Outcome<HttpResponse>::failure(
            {ErrorCode::Internal, "BeatFlow could not initialize its network client.", true, std::nullopt});
    }

    CurlHeaders headers;
    for (const auto& [name, value] : request.headers) {
        if (!headers.append(name + ": " + value)) {
            return Outcome<HttpResponse>::failure(
                {ErrorCode::Internal, "BeatFlow could not prepare the request headers.", true, std::nullopt});
        }
    }

    std::string body;
    std::string rawHeaders;
    std::array<char, CURL_ERROR_SIZE> errorDetails{};
    curl_easy_setopt(curl.get(), CURLOPT_URL, request.url.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, headers.get());
    curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT, static_cast<long>(request.timeoutSeconds));
    curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, static_cast<long>(request.timeoutSeconds));
    curl_easy_setopt(curl.get(), CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl.get(), CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, appendBytes);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(curl.get(), CURLOPT_HEADERFUNCTION, appendBytes);
    curl_easy_setopt(curl.get(), CURLOPT_HEADERDATA, &rawHeaders);
    curl_easy_setopt(curl.get(), CURLOPT_ERRORBUFFER, errorDetails.data());
    curl_easy_setopt(curl.get(), CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl.get(), CURLOPT_XFERINFOFUNCTION, cancelTransfer);
    curl_easy_setopt(curl.get(), CURLOPT_XFERINFODATA, &cancellation);

    if (request.method == HttpRequest::Method::Post) {
        curl_easy_setopt(curl.get(), CURLOPT_POST, 1L);
        curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDS, request.body.data());
        curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDSIZE_LARGE,
                         static_cast<curl_off_t>(request.body.size()));
    } else {
        curl_easy_setopt(curl.get(), CURLOPT_HTTPGET, 1L);
    }

    const auto curlStatus = curl_easy_perform(curl.get());
    if (cancellation.isCancellationRequested()) {
        return Outcome<HttpResponse>::failure(
            {ErrorCode::Cancelled, "The network request was cancelled.", false, std::nullopt});
    }
    if (curlStatus != CURLE_OK) {
        return Outcome<HttpResponse>::failure(curlError(curlStatus, errorDetails));
    }

    long status = 0;
    if (curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &status) != CURLE_OK || status < 0 ||
        status > std::numeric_limits<int>::max()) {
        return Outcome<HttpResponse>::failure(
            {ErrorCode::Network, "The server returned an invalid HTTP status.", true, std::nullopt});
    }

    HttpResponse translated;
    translated.status = static_cast<int>(status);
    if (translated.status == 0) {
        translated.status = http::statusFromRawHeaders(rawHeaders);
    }
    if (translated.status == 0) {
        return Outcome<HttpResponse>::failure(
            {ErrorCode::Network, "The server completed without an HTTP response.", true, std::nullopt});
    }
    translated.headers = http::parseHeaders(rawHeaders);
    translated.body = std::move(body);
    return Outcome<HttpResponse>::success(std::move(translated));
}

} // namespace beatflow::quest
