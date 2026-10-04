#include "beatnext/core/RecommendationSession.hpp"

#include <algorithm>

namespace beatnext {

std::uint64_t RecommendationSession::begin(std::string context) {
    RecommendationSessionState snapshot;
    std::vector<Callback> subscribers;
    {
        std::scoped_lock lock(mutex_);
        const auto generation = state_.generation + 1;
        state_ = {};
        state_.generation = generation;
        state_.context = std::move(context);
        state_.loading = true;
        snapshot = state_;
        subscribers = subscribersLocked();
    }
    notify(snapshot, subscribers);
    return snapshot.generation;
}

bool RecommendationSession::updateProgress(std::uint64_t generation, const RecommendationProgress& progress) {
    RecommendationSessionState snapshot;
    std::vector<Callback> subscribers;
    {
        std::scoped_lock lock(mutex_);
        if (generation != state_.generation || !state_.loading)
            return false;
        state_.progressStage = progress.stage;
        if (progress.sourceTrack)
            state_.sourceTrack = progress.sourceTrack;
        state_.completedTracks = progress.completedTracks;
        state_.totalTracks = progress.totalTracks;
        state_.matchesFound = progress.matchesFound;
        snapshot = state_;
        subscribers = subscribersLocked();
    }
    notify(snapshot, subscribers);
    return true;
}

bool RecommendationSession::finish(std::uint64_t generation, std::vector<RecommendedMap> recommendations,
                                   std::optional<ServiceError> error) {
    RecommendationSessionState snapshot;
    std::vector<Callback> subscribers;
    {
        std::scoped_lock lock(mutex_);
        if (generation != state_.generation)
            return false;
        state_.loading = false;
        state_.error = std::move(error);
        state_.items.clear();
        for (auto& recommendation : recommendations) {
            const auto status = recommendation.installed ? RecommendationItemStatus::Installed
                                                         : RecommendationItemStatus::Ready;
            state_.items.push_back({std::move(recommendation), status, {}});
        }
        state_.stale = std::ranges::any_of(state_.items, [](const auto& item) {
            return item.recommendation.track.stale || item.recommendation.map.stale;
        });
        state_.selectedIndex = state_.items.empty() ? std::nullopt : std::optional<std::size_t>(0);
        snapshot = state_;
        subscribers = subscribersLocked();
    }
    notify(snapshot, subscribers);
    return true;
}

bool RecommendationSession::select(std::size_t index) {
    RecommendationSessionState snapshot;
    std::vector<Callback> subscribers;
    {
        std::scoped_lock lock(mutex_);
        if (index >= state_.items.size())
            return false;
        state_.selectedIndex = index;
        snapshot = state_;
        subscribers = subscribersLocked();
    }
    notify(snapshot, subscribers);
    return true;
}

bool RecommendationSession::updateItem(std::uint64_t generation, std::size_t index,
                                       RecommendationItemStatus status, std::string message, bool installed) {
    RecommendationSessionState snapshot;
    std::vector<Callback> subscribers;
    {
        std::scoped_lock lock(mutex_);
        if (generation != state_.generation || index >= state_.items.size())
            return false;
        auto& item = state_.items[index];
        item.status = status;
        item.message = std::move(message);
        if (installed)
            item.recommendation.installed = true;
        snapshot = state_;
        subscribers = subscribersLocked();
    }
    notify(snapshot, subscribers);
    return true;
}

RecommendationSessionState RecommendationSession::state() const {
    std::scoped_lock lock(mutex_);
    return state_;
}

std::uint64_t RecommendationSession::subscribe(Callback callback) {
    RecommendationSessionState snapshot;
    std::uint64_t token = 0;
    {
        std::scoped_lock lock(mutex_);
        token = nextSubscription_++;
        subscribers_.emplace(token, callback);
        snapshot = state_;
    }
    callback(snapshot);
    return token;
}

void RecommendationSession::unsubscribe(std::uint64_t subscription) {
    std::scoped_lock lock(mutex_);
    subscribers_.erase(subscription);
}

void RecommendationSession::notify(const RecommendationSessionState& snapshot,
                                   const std::vector<Callback>& subscribers) const {
    for (const auto& subscriber : subscribers)
        subscriber(snapshot);
}

std::vector<RecommendationSession::Callback> RecommendationSession::subscribersLocked() const {
    std::vector<Callback> result;
    result.reserve(subscribers_.size());
    for (const auto& [_, subscriber] : subscribers_)
        result.push_back(subscriber);
    return result;
}

} // namespace beatnext
