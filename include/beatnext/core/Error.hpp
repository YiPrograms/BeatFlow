#pragma once

#include <optional>
#include <string>
#include <utility>

namespace beatnext {

enum class ErrorCode {
    Cancelled,
    Network,
    RateLimited,
    InvalidResponse,
    NotFound,
    Storage,
    Unsupported,
    Internal,
};

struct ServiceError {
    ErrorCode code{ErrorCode::Internal};
    std::string message;
    bool retryable{false};
    std::optional<int> retryAfterSeconds;
};

template <typename T> class Outcome {
  public:
    static Outcome success(T value) {
        return Outcome(std::move(value));
    }
    static Outcome failure(ServiceError error) {
        return Outcome(std::move(error));
    }

    [[nodiscard]] bool ok() const noexcept {
        return value_.has_value();
    }
    [[nodiscard]] explicit operator bool() const noexcept {
        return ok();
    }

    [[nodiscard]] const T& value() const& {
        return value_.value();
    }
    [[nodiscard]] T& value() & {
        return value_.value();
    }
    [[nodiscard]] T&& value() && {
        return std::move(value_).value();
    }
    [[nodiscard]] const ServiceError& error() const {
        return error_.value();
    }

  private:
    explicit Outcome(T value) : value_(std::move(value)) {}
    explicit Outcome(ServiceError error) : error_(std::move(error)) {}

    std::optional<T> value_;
    std::optional<ServiceError> error_;
};

} // namespace beatnext
