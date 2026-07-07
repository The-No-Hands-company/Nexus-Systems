#pragma once

#include <string>
#include <vector>
#include <functional>
#include <source_location>
#include <algorithm>
#include <mutex>

namespace nexus::utility::statistics {

class PercentileCalculator {
public:
    static PercentileCalculator& instance() {
        static PercentileCalculator inst;
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
        values_.clear();
        sorted_ = true;
    }

    void addValue(double value) {
        std::lock_guard<std::mutex> lock(mutex_);
        values_.push_back(value);
        sorted_ = false;
    }

    double getPercentile(double p) const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (values_.empty()) return 0.0;
        ensureSorted();
        if (p <= 0.0) return values_.front();
        if (p >= 100.0) return values_.back();
        double idx = (p / 100.0) * static_cast<double>(values_.size() - 1);
        size_t lo = static_cast<size_t>(idx);
        size_t hi = lo + 1;
        if (hi >= values_.size()) return values_[lo];
        double frac = idx - static_cast<double>(lo);
        return values_[lo] * (1.0 - frac) + values_[hi] * frac;
    }

    double getMedian() const {
        return getPercentile(50.0);
    }

    double getValueAtRank(size_t rank) const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (values_.empty()) return 0.0;
        ensureSorted();
        if (rank >= values_.size()) return values_.back();
        return values_[rank];
    }

    size_t getCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return values_.size();
    }

private:
    PercentileCalculator() = default;
    ~PercentileCalculator() = default;

    PercentileCalculator(const PercentileCalculator&) = delete;
    PercentileCalculator& operator=(const PercentileCalculator&) = delete;
    PercentileCalculator(PercentileCalculator&&) = delete;
    PercentileCalculator& operator=(PercentileCalculator&&) = delete;

    void ensureSorted() const {
        if (!sorted_) {
            std::sort(values_.begin(), values_.end());
            sorted_ = true;
        }
    }

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    mutable std::vector<double> values_;
    mutable bool sorted_ = true;
};

} // namespace nexus::utility::statistics
