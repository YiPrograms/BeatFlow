#include "beatnext/services/WorkerQueue.hpp"

#include <utility>

namespace beatnext {

WorkerQueue::WorkerQueue(std::size_t workerCount, std::size_t maximumQueuedTasks)
    : maximumQueuedTasks_(maximumQueuedTasks) {
    workerCount = std::max<std::size_t>(1, workerCount);
    workers_.reserve(workerCount);
    for (std::size_t index = 0; index < workerCount; ++index) {
        workers_.emplace_back([this] { run(); });
    }
}

WorkerQueue::~WorkerQueue() {
    stop();
}

bool WorkerQueue::submit(std::function<void()> task) {
    {
        std::lock_guard lock(mutex_);
        if (stopping_ || tasks_.size() >= maximumQueuedTasks_) {
            return false;
        }
        tasks_.push(std::move(task));
    }
    ready_.notify_one();
    return true;
}

void WorkerQueue::stop() {
    {
        std::lock_guard lock(mutex_);
        if (stopping_) {
            return;
        }
        stopping_ = true;
        std::queue<std::function<void()>> empty;
        tasks_.swap(empty);
    }
    ready_.notify_all();
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void WorkerQueue::run() {
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock lock(mutex_);
            ready_.wait(lock, [this] { return stopping_ || !tasks_.empty(); });
            if (stopping_) {
                return;
            }
            task = std::move(tasks_.front());
            tasks_.pop();
        }
        try {
            task();
        } catch (...) {
            // Task owners surface errors through their own completion callback.
        }
    }
}

} // namespace beatnext
