#pragma once

#include "beatnext/core/Error.hpp"
#include "beatnext/core/Models.hpp"

#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <optional>

namespace beatnext {

enum class RecommendationItemStatus { Ready, Downloading, Installed, Failed };

struct RecommendationItemState {
    RecommendedMap recommendation;
    RecommendationItemStatus status{RecommendationItemStatus::Ready};
    std::string message;
};

struct RecommendationSessionState {
    std::uint64_t generation{0};
    std::vector<RecommendationItemState> items;
    std::optional<ServiceError> error;
    std::string context;
    std::optional<std::size_t> selectedIndex;
    bool loading{false};
    bool stale{false};
};

class RecommendationSession {
  public:
    using Callback = std::function<void(const RecommendationSessionState&)>;

    [[nodiscard]] std::uint64_t begin(std::string context);
    bool finish(std::uint64_t generation, std::vector<RecommendedMap> recommendations,
                std::optional<ServiceError> error = {});
    bool select(std::size_t index);
    bool updateItem(std::uint64_t generation, std::size_t index, RecommendationItemStatus status,
                    std::string message = {}, bool installed = false);

    [[nodiscard]] RecommendationSessionState state() const;
    [[nodiscard]] std::uint64_t subscribe(Callback callback);
    void unsubscribe(std::uint64_t subscription);

  private:
    void notify(const RecommendationSessionState& snapshot, const std::vector<Callback>& subscribers) const;
    [[nodiscard]] std::vector<Callback> subscribersLocked() const;

    mutable std::mutex mutex_;
    RecommendationSessionState state_;
    std::map<std::uint64_t, Callback> subscribers_;
    std::uint64_t nextSubscription_{1};
};

} // namespace beatnext
