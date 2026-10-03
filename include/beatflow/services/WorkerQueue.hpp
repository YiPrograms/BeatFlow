#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace beatflow {

class WorkerQueue {
  public:
    explicit WorkerQueue(std::size_t workerCount = 2, std::size_t maximumQueuedTasks = 64);
    ~WorkerQueue();

    WorkerQueue(const WorkerQueue&) = delete;
    WorkerQueue& operator=(const WorkerQueue&) = delete;

    [[nodiscard]] bool submit(std::function<void()> task);
    void stop();

  private:
    void run();

    std::mutex mutex_;
    std::condition_variable ready_;
    std::queue<std::function<void()>> tasks_;
    std::vector<std::thread> workers_;
    std::size_t maximumQueuedTasks_;
    bool stopping_{false};
};

} // namespace beatflow
