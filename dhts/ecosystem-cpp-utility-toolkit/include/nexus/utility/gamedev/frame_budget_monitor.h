#pragma once

#include <string>
#include <vector>
#include <functional>
#include <source_location>
#include <mutex>

namespace nexus::utility::gamedev {

class FrameBudgetMonitor {
public:
    static FrameBudgetMonitor& instance() {
        static FrameBudgetMonitor inst;
        return inst;
    }

    void initialize(const std::string& config = "") { config_ = config; enabled_ = true; }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_; }
    void enable() { enabled_ = true; }
    void disable() { enabled_ = false; }
    std::string getStatus() const { return enabled_ ? "active" : "inactive"; }
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        budget_ms_ = 16.67;
        total_frames_ = 0;
        over_budget_count_ = 0;
        total_frame_time_ = 0.0;
        worst_frame_ = 0.0;
    }

    void setBudget(double ms_per_frame) {
        std::lock_guard<std::mutex> lock(mutex_);
        budget_ms_ = ms_per_frame;
    }

    void recordFrame(double actual_ms) {
        std::lock_guard<std::mutex> lock(mutex_);
        ++total_frames_;
        total_frame_time_ += actual_ms;
        if (actual_ms > worst_frame_) worst_frame_ = actual_ms;
        if (actual_ms > budget_ms_) ++over_budget_count_;
    }

    bool isOverBudget() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return total_frames_ > 0 && (total_frame_time_ / static_cast<double>(total_frames_)) > budget_ms_;
    }

    size_t getOverBudgetCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return over_budget_count_;
    }

    double getAverageFrameTime() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (total_frames_ == 0) return 0.0;
        return total_frame_time_ / static_cast<double>(total_frames_);
    }

    double getWorstFrame() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return worst_frame_;
    }

    size_t getFrameCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return total_frames_;
    }

private:
    FrameBudgetMonitor() = default;
    ~FrameBudgetMonitor() = default;

    FrameBudgetMonitor(const FrameBudgetMonitor&) = delete;
    FrameBudgetMonitor& operator=(const FrameBudgetMonitor&) = delete;
    FrameBudgetMonitor(FrameBudgetMonitor&&) = delete;
    FrameBudgetMonitor& operator=(FrameBudgetMonitor&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    double budget_ms_ = 16.67;
    size_t total_frames_ = 0;
    size_t over_budget_count_ = 0;
    double total_frame_time_ = 0.0;
    double worst_frame_ = 0.0;
};

} // namespace nexus::utility::gamedev
