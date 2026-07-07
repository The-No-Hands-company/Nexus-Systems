#pragma once

#include <string>
#include <vector>
#include <cmath>
#include <atomic>
#include <mutex>

namespace nexus::utility::scientific {

/**
 * @brief Monitor a simulation for diverging (NaN/Inf/exploding) values.
 */
class SimulationStabilityMonitor {
public:
    enum class Status { Stable, Growing, Diverged };

    struct Report {
        Status status = Status::Stable;
        double lastValue = 0.0;
        double growthRate = 0.0;
        std::size_t step = 0;
    };

    static SimulationStabilityMonitor& instance() {
        static SimulationStabilityMonitor inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    void setThreshold(double divergenceThreshold) {
        std::lock_guard<std::mutex> lk(mutex_);
        threshold_ = divergenceThreshold;
    }

    Report observe(double value) {
        std::lock_guard<std::mutex> lk(mutex_);
        ++step_;
        Report r;
        r.step = step_;
        r.lastValue = value;
        if (std::isnan(value) || std::isinf(value) || std::fabs(value) > threshold_) {
            r.status = Status::Diverged;
            diverged_ = true;
        } else {
            if (step_ > 1 && prev_ != 0.0) {
                r.growthRate = std::fabs(value) / std::fabs(prev_);
                if (r.growthRate > 1.0) r.status = Status::Growing;
            }
        }
        prev_ = value;
        lastReport_ = r;
        return r;
    }

    bool hasDiverged() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return diverged_;
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        step_ = 0;
        prev_ = 0.0;
        diverged_ = false;
        lastReport_ = {};
    }

private:
    SimulationStabilityMonitor() = default;
    ~SimulationStabilityMonitor() = default;
    SimulationStabilityMonitor(const SimulationStabilityMonitor&) = delete;
    SimulationStabilityMonitor& operator=(const SimulationStabilityMonitor&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    double threshold_ = 1e100;
    std::size_t step_ = 0;
    double prev_ = 0.0;
    bool diverged_ = false;
    Report lastReport_;
};

} // namespace nexus::utility::scientific
