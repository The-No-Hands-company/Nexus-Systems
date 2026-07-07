#pragma once

#include <string>
#include <vector>
#include <functional>
#include <source_location>
#include <cmath>
#include <mutex>

namespace nexus::utility::statistics {

class CorrelationTracker {
public:
    static CorrelationTracker& instance() {
        static CorrelationTracker inst;
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
        count_ = 0;
        sum_x_ = 0.0;
        sum_y_ = 0.0;
        sum_xy_ = 0.0;
        sum_x2_ = 0.0;
        sum_y2_ = 0.0;
    }

    void addDataPoint(double x, double y) {
        std::lock_guard<std::mutex> lock(mutex_);
        ++count_;
        sum_x_ += x;
        sum_y_ += y;
        sum_xy_ += x * y;
        sum_x2_ += x * x;
        sum_y2_ += y * y;
    }

    double getCorrelation() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (count_ < 2) return 0.0;
        double n = static_cast<double>(count_);
        double numerator = n * sum_xy_ - sum_x_ * sum_y_;
        double denom_x = std::sqrt(n * sum_x2_ - sum_x_ * sum_x_);
        double denom_y = std::sqrt(n * sum_y2_ - sum_y_ * sum_y_);
        double denominator = denom_x * denom_y;
        if (denominator < 1e-10) return 0.0;
        return numerator / denominator;
    }

    size_t getCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return count_;
    }

private:
    CorrelationTracker() = default;
    ~CorrelationTracker() = default;

    CorrelationTracker(const CorrelationTracker&) = delete;
    CorrelationTracker& operator=(const CorrelationTracker&) = delete;
    CorrelationTracker(CorrelationTracker&&) = delete;
    CorrelationTracker& operator=(CorrelationTracker&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    size_t count_ = 0;
    double sum_x_ = 0.0;
    double sum_y_ = 0.0;
    double sum_xy_ = 0.0;
    double sum_x2_ = 0.0;
    double sum_y2_ = 0.0;
};

} // namespace nexus::utility::statistics
