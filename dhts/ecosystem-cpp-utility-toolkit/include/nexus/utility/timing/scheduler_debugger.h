#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <unordered_map>
#include <mutex>

namespace nexus::utility::timing {

/// @brief Debug task scheduling by measuring scheduled-vs-executed delay.
class SchedulerDebugger {
public:
    using TimePoint = std::chrono::steady_clock::time_point;

    static SchedulerDebugger& instance() {
        static SchedulerDebugger inst;
        return inst;
    }

    void initialize() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = true;
        tasks_.clear();
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

    void recordSchedule(const std::string& task, TimePoint scheduled) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto& info = tasks_[task];
        info.scheduled = scheduled;
        info.has_scheduled = true;
    }

    void recordExecute(const std::string& task, TimePoint executed) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = tasks_.find(task);
        if (it == tasks_.end() || !it->second.has_scheduled) return;
        auto delay = std::chrono::duration_cast<std::chrono::microseconds>(
            executed - it->second.scheduled);
        if (delay.count() < 0) delay = std::chrono::microseconds{0};
        it->second.last_delay = delay;
        it->second.total_delay += delay;
        ++it->second.samples;
    }

    /// Most recent scheduling delay for a task.
    std::chrono::microseconds getScheduleDelay(const std::string& task) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = tasks_.find(task);
        return it != tasks_.end() ? it->second.last_delay : std::chrono::microseconds{0};
    }

    /// Average scheduling delay across all tasks and samples.
    std::chrono::microseconds getAverageDelay() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::chrono::microseconds total{0};
        size_t samples = 0;
        for (const auto& [name, info] : tasks_) {
            total += info.total_delay;
            samples += info.samples;
        }
        if (samples == 0) return std::chrono::microseconds{0};
        return std::chrono::microseconds{total.count() / static_cast<long long>(samples)};
    }

private:
    struct TaskInfo {
        TimePoint scheduled{};
        bool has_scheduled = false;
        std::chrono::microseconds last_delay{0};
        std::chrono::microseconds total_delay{0};
        size_t samples = 0;
    };

    SchedulerDebugger() = default;
    ~SchedulerDebugger() = default;
    SchedulerDebugger(const SchedulerDebugger&) = delete;
    SchedulerDebugger& operator=(const SchedulerDebugger&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::unordered_map<std::string, TaskInfo> tasks_;
};

} // namespace nexus::utility::timing
