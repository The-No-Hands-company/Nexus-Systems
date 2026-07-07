#pragma once

#include <queue>
#include <vector>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <atomic>
#include <future>
#include <map>
#include <string>
#include <cstdint>
#include <iostream>
#include <exception>
#include <memory>

namespace nexus::utility::scheduling {

/// Priority-based task scheduler with delayed execution
class TaskScheduler {
public:
    enum class Priority { Low = 0, Normal = 1, High = 2, Critical = 3 };

    struct Task {
        int64_t id;
        Priority priority;
        std::chrono::steady_clock::time_point scheduled_at;
        std::function<void()> work;

        bool operator<(const Task& other) const {
            if (scheduled_at != other.scheduled_at)
                return scheduled_at > other.scheduled_at; // earlier first
            return static_cast<int>(priority) < static_cast<int>(other.priority);
        }
    };

    explicit TaskScheduler(size_t worker_count = 1) : worker_count_(worker_count) {}

    ~TaskScheduler() { stop(); }

    /// Start the scheduler with worker threads
    void start() {
        running_ = true;
        for (size_t i = 0; i < worker_count_; ++i) {
            workers_.emplace_back(&TaskScheduler::workerLoop, this);
        }
    }

    /// Stop the scheduler gracefully
    void stop() {
        running_ = false;
        cv_.notify_all();
        for (auto& w : workers_) {
            if (w.joinable()) w.join();
        }
        workers_.clear();
    }

    /// Schedule a task for immediate execution
    int64_t schedule(std::function<void()> work, Priority priority = Priority::Normal) {
        return scheduleAt(std::move(work),
                          std::chrono::steady_clock::now(), priority);
    }

    /// Schedule a task for delayed execution
    int64_t scheduleDelayed(std::function<void()> work,
                             std::chrono::milliseconds delay,
                             Priority priority = Priority::Normal) {
        return scheduleAt(std::move(work),
                          std::chrono::steady_clock::now() + delay, priority);
    }

    /// Schedule a periodic task (cancellable via cancelTask)
    int64_t schedulePeriodic(std::function<void()> work,
                              std::chrono::milliseconds interval,
                              Priority priority = Priority::Normal) {
        int64_t id = next_id_.fetch_add(1, std::memory_order_relaxed);
        auto cancel_flag = std::make_shared<std::atomic<bool>>(false);
        {
            std::lock_guard lock(mutex_);
            periodic_tasks_[id] = cancel_flag;
        }

        scheduleAt(id, wrapPeriodic(id, std::move(work), interval, priority, cancel_flag),
                   std::chrono::steady_clock::now() + interval, priority);
        return id;
    }

    /// Cancel a periodic task by ID. Returns true if task was found and cancelled.
    bool cancelTask(int64_t id) noexcept {
        std::lock_guard lock(mutex_);
        auto it = periodic_tasks_.find(id);
        if (it != periodic_tasks_.end()) {
            it->second->store(true, std::memory_order_release);
            periodic_tasks_.erase(it);
            return true;
        }
        return false;
    }

    /// Get pending task count
    [[nodiscard]] size_t pendingCount() const noexcept {
        std::lock_guard lock(mutex_);
        return tasks_.size();
    }

    /// Check if scheduler is running
    [[nodiscard]] bool isRunning() const noexcept { return running_; }

private:
    std::priority_queue<Task> tasks_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<std::thread> workers_;
    std::atomic<bool> running_{false};
    std::atomic<int64_t> next_id_{1};
    size_t worker_count_;
    std::map<int64_t, std::shared_ptr<std::atomic<bool>>> periodic_tasks_;

    int64_t scheduleAt(std::function<void()> work,
                        std::chrono::steady_clock::time_point when,
                        Priority priority) {
        int64_t id = next_id_.fetch_add(1, std::memory_order_relaxed);
        scheduleAt(id, std::move(work), when, priority);
        return id;
    }

    void scheduleAt(int64_t id, std::function<void()> work,
                    std::chrono::steady_clock::time_point when,
                    Priority priority) {
        std::lock_guard lock(mutex_);
        tasks_.push({id, priority, when, std::move(work)});
        cv_.notify_one();
    }

    std::function<void()> wrapPeriodic(int64_t id,
                                       std::function<void()> work,
                                       std::chrono::milliseconds interval,
                                       Priority priority,
                                       std::shared_ptr<std::atomic<bool>> cancel_flag) {
        return [this, id, work = std::move(work), interval, priority,
                cancel_flag = std::move(cancel_flag)]() mutable {
            if (cancel_flag->load(std::memory_order_acquire)) {
                return;  // Cancelled, don't execute
            }
            work();
            if (running_.load(std::memory_order_acquire) &&
                !cancel_flag->load(std::memory_order_acquire)) {
                std::lock_guard lock(mutex_);
                tasks_.push({id, priority,
                             std::chrono::steady_clock::now() + interval,
                             wrapPeriodic(id, std::move(work), interval, priority, cancel_flag)});
                cv_.notify_one();
            }
        };
    }

    void workerLoop() {
        while (running_) {
            std::unique_lock lock(mutex_);
            if (tasks_.empty()) {
                cv_.wait(lock, [this] { return !tasks_.empty() || !running_; });
                if (!running_) return;
            }

            auto task = tasks_.top();
            auto now = std::chrono::steady_clock::now();
            if (task.scheduled_at > now) {
                auto waiting_for_id = task.id;
                cv_.wait_until(lock, task.scheduled_at,
                               [this, waiting_for_id] {
                                   return !running_.load(std::memory_order_acquire) ||
                                          tasks_.empty() ||
                                          tasks_.top().id != waiting_for_id;
                               });
                continue;
            }

            tasks_.pop();
            lock.unlock();

            try {
                task.work();
            } catch (const std::exception& e) {
                std::cerr << "[TaskScheduler] Task #" << task.id
                          << " failed: " << e.what() << "\n";
            } catch (...) {
                std::cerr << "[TaskScheduler] Task #" << task.id
                          << " failed with unknown exception\n";
            }
        }
    }
};

} // namespace nexus::utility::scheduling
