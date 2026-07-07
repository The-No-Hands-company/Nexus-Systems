#pragma once

#include <cmath>
#include <cstdint>
#include <mutex>
#include <string>

namespace nexus::utility::algorithmic {

class ConvergenceMonitor {
public:
    static ConvergenceMonitor& instance() {
        static ConvergenceMonitor inst;
        return inst;
    }

    void initialize() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = true; }
    void shutdown() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = false; reset(); }
    bool isEnabled() const { return enabled_; }

    void recordIteration(double value) {
        std::lock_guard<std::mutex> lock(mutex_);
        prev_value_ = current_value_;
        current_value_ = value;
        ++iteration_count_;
        if (iteration_count_ > 1) {
            double delta = std::abs(current_value_ - prev_value_);
            convergence_rate_ = delta > 0.0 ? 1.0 / delta : 0.0;
            if (convergence_rate_ > max_convergence_rate_) max_convergence_rate_ = convergence_rate_;
        }
    }

    bool hasConverged(double epsilon) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return iteration_count_ > 1 && std::abs(current_value_ - prev_value_) < epsilon;
    }

    uint64_t getIterationCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return iteration_count_;
    }

    double getConvergenceRate() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return convergence_rate_;
    }

private:
    void reset() {
        current_value_ = 0.0;
        prev_value_ = 0.0;
        iteration_count_ = 0;
        convergence_rate_ = 0.0;
        max_convergence_rate_ = 0.0;
    }

    ConvergenceMonitor() = default;
    ~ConvergenceMonitor() = default;
    ConvergenceMonitor(const ConvergenceMonitor&) = delete;
    ConvergenceMonitor& operator=(const ConvergenceMonitor&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    double current_value_ = 0.0;
    double prev_value_ = 0.0;
    uint64_t iteration_count_ = 0;
    double convergence_rate_ = 0.0;
    double max_convergence_rate_ = 0.0;
};

} // namespace nexus::utility::algorithmic
