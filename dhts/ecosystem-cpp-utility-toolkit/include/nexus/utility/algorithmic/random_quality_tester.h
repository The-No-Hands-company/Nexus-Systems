#pragma once

#include <cmath>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace nexus::utility::algorithmic {

class RandomQualityTester {
public:
    static RandomQualityTester& instance() {
        static RandomQualityTester inst;
        return inst;
    }

    void initialize() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = true; }
    void shutdown() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = false; reset(); }
    bool isEnabled() const { return enabled_; }

    void addSample(uint64_t value) {
        std::lock_guard<std::mutex> lock(mutex_);
        samples_.push_back(value);
        sum_ += value;
        sum_sq_ += static_cast<double>(value) * value;
        ++count_;
    }

    double getMean() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return count_ > 0 ? sum_ / count_ : 0.0;
    }

    double getVariance() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (count_ < 2) return 0.0;
        double mean = sum_ / count_;
        return (sum_sq_ / count_) - (mean * mean);
    }

    int runsTest() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (samples_.size() < 2) return 0;
        double median = sum_ / count_;
        int runs = 1;
        bool above = (samples_[0] >= median);
        for (size_t i = 1; i < samples_.size(); ++i) {
            bool current_above = (samples_[i] >= median);
            if (current_above != above) {
                ++runs;
                above = current_above;
            }
        }
        return runs;
    }

    double getAutoCorrelation(size_t lag) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (count_ < 2 || lag >= samples_.size()) return 0.0;
        double mean = sum_ / count_;
        double numerator = 0.0;
        double denominator = 0.0;
        for (size_t i = 0; i < count_; ++i) {
            double diff = static_cast<double>(samples_[i]) - mean;
            denominator += diff * diff;
        }
        if (denominator == 0.0) return 0.0;
        for (size_t i = 0; i < count_ - lag; ++i) {
            numerator += (static_cast<double>(samples_[i]) - mean) *
                         (static_cast<double>(samples_[i + lag]) - mean);
        }
        return numerator / denominator;
    }

    uint64_t getCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return count_;
    }

private:
    void reset() {
        samples_.clear();
        sum_ = 0.0;
        sum_sq_ = 0.0;
        count_ = 0;
    }

    RandomQualityTester() = default;
    ~RandomQualityTester() = default;
    RandomQualityTester(const RandomQualityTester&) = delete;
    RandomQualityTester& operator=(const RandomQualityTester&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::vector<uint64_t> samples_;
    double sum_ = 0.0;
    double sum_sq_ = 0.0;
    uint64_t count_ = 0;
};

} // namespace nexus::utility::algorithmic
