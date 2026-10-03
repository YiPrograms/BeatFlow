#pragma once

#include "beatflow/core/Interfaces.hpp"

#include <chrono>
#include <functional>

namespace beatflow {

struct RetryPolicy {
    int maximumAttempts{3};
    std::chrono::milliseconds initialDelay{250};
    std::chrono::milliseconds maximumDelay{5000};
};

// Keeps retry policy out of provider adapters. The wrapped client performs one
// request; this decorator owns bounded backoff and honours server Retry-After.
class RetryingHttpClient final : public HttpClient {
  public:
    using Sleeper = std::function<bool(std::chrono::milliseconds, const CancellationToken&)>;

    explicit RetryingHttpClient(HttpClient& inner, RetryPolicy policy = {}, Sleeper sleeper = {});

    Outcome<HttpResponse> send(const HttpRequest& request, const CancellationToken& cancellation) override;

  private:
    [[nodiscard]] static bool shouldRetry(const Outcome<HttpResponse>& outcome);
    [[nodiscard]] std::chrono::milliseconds delayFor(const Outcome<HttpResponse>& outcome,
                                                     int completedAttempts) const;

    HttpClient& inner_;
    RetryPolicy policy_;
    Sleeper sleeper_;
};

} // namespace beatflow
