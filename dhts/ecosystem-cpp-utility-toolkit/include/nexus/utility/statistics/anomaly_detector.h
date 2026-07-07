#pragma once

#include <string>
#include <vector>
#include <functional>
#include <source_location>
#include <cmath>
#include <mutex>

namespace nexus::utility::statistics {

class AnomalyDetector {
public:
    static AnomalyDetector& instance() {
        static AnomalyDetector inst;
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
        mean_ = 0.0;
        m2_ = 0.0;
    }

    void addDataPoint(double value) {
        std::lock_guard<std::mutex> lock(mutex_);
        ++count_;
        double delta = value - mean_;
        mean_ += delta / static_cast<double>(count_);
        double delta2 = value - mean_;
        m2_ += delta * delta2;
    }

    bool isAnomaly(double value, double threshold = 3.0) const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (count_ < 2) return false;
        double variance = m2_ / static_cast<double>(count_);
        double stddev = std::sqrt(variance);
        if (stddev < 1e-10) return false;
        double zscore = std::abs((value - mean_) / stddev);
        return zscore > threshold;
    }

    double getMean() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return mean_;
    }

    double getStdDev() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (count_ < 2) return 0.0;
        return std::sqrt(m2_ / static_cast<double>(count_));
    }

    size_t getCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return count_;
    }

private:
    AnomalyDetector() = default;
    ~AnomalyDetector() = default;

    AnomalyDetector(const AnomalyDetector&) = delete;
    AnomalyDetector& operator=(const AnomalyDetector&) = delete;
    AnomalyDetector(AnomalyDetector&&) = delete;
    AnomalyDetector& operator=(AnomalyDetector&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    size_t count_ = 0;
    double mean_ = 0.0;
    double m2_ = 0.0;
};

} // namespace nexus::utility::statistics
