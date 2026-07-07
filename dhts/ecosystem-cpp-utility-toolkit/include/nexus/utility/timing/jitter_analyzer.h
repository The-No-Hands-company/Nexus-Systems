#pragma once

#include <chrono>
#include <vector>
#include <mutex>
#include <cmath>
#include <cstdlib>

namespace nexus::utility::timing {

/// @brief Analyze timing jitter as deviation between expected and actual intervals.
class JitterAnalyzer {
public:
    static JitterAnalyzer& instance() {
        static JitterAnalyzer inst;
        return inst;
    }

    void initialize() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = true;
        deviations_.clear();
    }

    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
        deviations_.clear();
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    /// Record an interval; jitter is the absolute deviation actual - expected (microseconds).
    void recordInterval(std::chrono::microseconds expected, std::chrono::microseconds actual) {
        std::lock_guard<std::mutex> lock(mutex_);
        deviations_.push_back(std::llabs((actual - expected).count()));
    }

    /// Average absolute jitter in microseconds.
    double getAverageJitter() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (deviations_.empty()) return 0.0;
        long long sum = 0;
        for (auto d : deviations_) sum += d;
        return static_cast<double>(sum) / static_cast<double>(deviations_.size());
    }

    /// Maximum absolute jitter in microseconds.
    long long getMaxJitter() const {
        std::lock_guard<std::mutex> lock(mutex_);
        long long m = 0;
        for (auto d : deviations_) {
            if (d > m) m = d;
        }
        return m;
    }

    /// Population standard deviation of jitter in microseconds.
    double getJitterStdDev() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (deviations_.empty()) return 0.0;
        double n = static_cast<double>(deviations_.size());
        double mean = 0.0;
        for (auto d : deviations_) mean += static_cast<double>(d);
        mean /= n;
        double var = 0.0;
        for (auto d : deviations_) {
            double diff = static_cast<double>(d) - mean;
            var += diff * diff;
        }
        var /= n;
        return std::sqrt(var);
    }

private:
    JitterAnalyzer() = default;
    ~JitterAnalyzer() = default;
    JitterAnalyzer(const JitterAnalyzer&) = delete;
    JitterAnalyzer& operator=(const JitterAnalyzer&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::vector<long long> deviations_;
};

} // namespace nexus::utility::timing
