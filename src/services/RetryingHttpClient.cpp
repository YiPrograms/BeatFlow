#include "beatflow/services/RetryingHttpClient.hpp"

#include <algorithm>
#include <charconv>
#include <limits>
#include <string_view>
#include <thread>

namespace beatflow {
namespace {

bool defaultSleep(std::chrono::milliseconds delay, const CancellationToken& cancellation) {
    constexpr auto quantum = std::chrono::milliseconds(50);
    auto remaining = delay;
    while (remaining.count() > 0) {
        if (cancellation.isCancellationRequested()) {
            return false;
        }
        const auto slice = std::min(remaining, quantum);
        std::this_thread::sleep_for(slice);
        remaining -= slice;
    }
    return !cancellation.isCancellationRequested();
}

std::optional<int> retryAfterSeconds(const HttpResponse& response) {
    for (const auto& [name, value] : response.headers) {
        if (name != "Retry-After" && name != "retry-after") {
            continue;
        }
        int seconds = 0;
        const auto parsed = std::from_chars(value.data(), value.data() + value.size(), seconds);
        if (parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size() && seconds >= 0) {
            return seconds;
        }
    }
    return std::nullopt;
}

} // namespace

RetryingHttpClient::RetryingHttpClient(HttpClient& inner, RetryPolicy policy, Sleeper sleeper)
    : inner_(inner), policy_(policy), sleeper_(std::move(sleeper)) {
    policy_.maximumAttempts = std::max(1, policy_.maximumAttempts);
    policy_.initialDelay = std::max(std::chrono::milliseconds::zero(), policy_.initialDelay);
    policy_.maximumDelay = std::max(policy_.initialDelay, policy_.maximumDelay);
    if (!sleeper_) {
        sleeper_ = defaultSleep;
    }
}

Outcome<HttpResponse> RetryingHttpClient::send(const HttpRequest& request,
                                               const CancellationToken& cancellation) {
    for (int attempt = 1; attempt <= policy_.maximumAttempts; ++attempt) {
        if (cancellation.isCancellationRequested()) {
            return Outcome<HttpResponse>::failure(
                {ErrorCode::Cancelled, "The HTTP request was cancelled.", false, std::nullopt});
        }

        auto outcome = inner_.send(request, cancellation);
        if (attempt == policy_.maximumAttempts || !shouldRetry(outcome)) {
            return outcome;
        }
        if (!sleeper_(delayFor(outcome, attempt), cancellation)) {
            return Outcome<HttpResponse>::failure(
                {ErrorCode::Cancelled, "The HTTP retry was cancelled.", false, std::nullopt});
        }
    }
    return Outcome<HttpResponse>::failure(
        {ErrorCode::Internal, "The HTTP retry loop ended unexpectedly.", false, std::nullopt});
}

bool RetryingHttpClient::shouldRetry(const Outcome<HttpResponse>& outcome) {
    if (!outcome) {
        return outcome.error().retryable;
    }
    const auto status = outcome.value().status;
    return status == 429 || status == 408 || status >= 500;
}

std::chrono::milliseconds RetryingHttpClient::delayFor(const Outcome<HttpResponse>& outcome,
                                                       int completedAttempts) const {
    if (outcome) {
        if (const auto directed = retryAfterSeconds(outcome.value())) {
            const auto milliseconds = std::chrono::seconds(*directed);
            return std::min(std::chrono::duration_cast<std::chrono::milliseconds>(milliseconds),
                            policy_.maximumDelay);
        }
    } else if (outcome.error().retryAfterSeconds) {
        const auto milliseconds = std::chrono::seconds(*outcome.error().retryAfterSeconds);
        return std::min(std::chrono::duration_cast<std::chrono::milliseconds>(milliseconds),
                        policy_.maximumDelay);
    }

    const auto shift = std::min(completedAttempts - 1, 20);
    const auto multiplier = 1LL << shift;
    const auto maximumCount = static_cast<long long>(policy_.maximumDelay.count());
    const auto delayedCount = policy_.initialDelay.count() > maximumCount / multiplier
                                  ? maximumCount
                                  : policy_.initialDelay.count() * multiplier;
    return std::chrono::milliseconds(std::min<long long>(delayedCount, maximumCount));
}

} // namespace beatflow
