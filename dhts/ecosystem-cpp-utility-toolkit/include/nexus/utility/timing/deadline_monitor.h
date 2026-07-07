#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <unordered_map>
#include <mutex>

namespace nexus::utility::timing {

/// @brief Monitor real-time task deadlines and track misses per task.
class DeadlineMonitor {
public:
    static DeadlineMonitor& instance() {
        static DeadlineMonitor inst;
        return inst;
    }

    void initialize() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = true;
    }

    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
        tasks_.clear();
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    void setDeadline(const std::string& task, std::chrono::milliseconds deadline) {
        std::lock_guard<std::mutex> lock(mutex_);
        tasks_[task].deadline = deadline;
    }

    /// Returns true if elapsed <= deadline. Records a miss otherwise.
    bool checkDeadline(const std::string& task, std::chrono::milliseconds elapsed) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto& info = tasks_[task];
        ++info.checks;
        bool met = elapsed <= info.deadline;
        if (!met) ++info.missed;
        return met;
    }

    /// Names of tasks that have missed their deadline at least once.
    std::vector<std::string> getMissedDeadlines() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> missed;
        for (const auto& [name, info] : tasks_) {
            if (info.missed > 0) missed.push_back(name);
        }
        return missed;
    }

    size_t getMissCount(const std::string& task) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = tasks_.find(task);
        return it != tasks_.end() ? it->second.missed : 0;
    }

private:
    struct TaskInfo {
        std::chrono::milliseconds deadline{0};
        size_t checks = 0;
        size_t missed = 0;
    };

    DeadlineMonitor() = default;
    ~DeadlineMonitor() = default;
    DeadlineMonitor(const DeadlineMonitor&) = delete;
    DeadlineMonitor& operator=(const DeadlineMonitor&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::unordered_map<std::string, TaskInfo> tasks_;
};

} // namespace nexus::utility::timing
