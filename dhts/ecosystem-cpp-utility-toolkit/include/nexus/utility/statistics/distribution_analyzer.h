#pragma once

#include <string>
#include <vector>
#include <functional>
#include <source_location>
#include <cmath>
#include <algorithm>
#include <mutex>

namespace nexus::utility::statistics {

class DistributionAnalyzer {
public:
    static DistributionAnalyzer& instance() {
        static DistributionAnalyzer inst;
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
        samples_.clear();
        sorted_ = true;
    }

    void addSample(double value) {
        std::lock_guard<std::mutex> lock(mutex_);
        samples_.push_back(value);
        sorted_ = false;
    }

    double getMin() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (samples_.empty()) return 0.0;
        return *std::min_element(samples_.begin(), samples_.end());
    }

    double getMax() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (samples_.empty()) return 0.0;
        return *std::max_element(samples_.begin(), samples_.end());
    }

    double getMean() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (samples_.empty()) return 0.0;
        double sum = 0.0;
        for (double v : samples_) sum += v;
        return sum / static_cast<double>(samples_.size());
    }

    double getMedian() const {
        return getPercentile(50.0);
    }

    double getPercentile(double p) const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (samples_.empty()) return 0.0;
        ensureSorted();
        if (p <= 0.0) return samples_.front();
        if (p >= 100.0) return samples_.back();
        double idx = (p / 100.0) * static_cast<double>(samples_.size() - 1);
        size_t lo = static_cast<size_t>(idx);
        size_t hi = lo + 1;
        if (hi >= samples_.size()) return samples_[lo];
        double frac = idx - static_cast<double>(lo);
        return samples_[lo] * (1.0 - frac) + samples_[hi] * frac;
    }

    size_t getCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return samples_.size();
    }

private:
    DistributionAnalyzer() = default;
    ~DistributionAnalyzer() = default;

    DistributionAnalyzer(const DistributionAnalyzer&) = delete;
    DistributionAnalyzer& operator=(const DistributionAnalyzer&) = delete;
    DistributionAnalyzer(DistributionAnalyzer&&) = delete;
    DistributionAnalyzer& operator=(DistributionAnalyzer&&) = delete;

    void ensureSorted() const {
        if (!sorted_) {
            std::sort(samples_.begin(), samples_.end());
            sorted_ = true;
        }
    }

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    mutable std::vector<double> samples_;
    mutable bool sorted_ = true;
};

} // namespace nexus::utility::statistics
