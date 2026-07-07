#pragma once

#include <string>
#include <vector>
#include <functional>
#include <source_location>
#include <algorithm>
#include <mutex>

namespace nexus::utility::statistics {

class OutlierDetector {
public:
    static OutlierDetector& instance() {
        static OutlierDetector inst;
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
        data_.clear();
        sorted_ = true;
    }

    void addValue(double value) {
        std::lock_guard<std::mutex> lock(mutex_);
        data_.push_back(value);
        sorted_ = false;
    }

    double getQ1() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (data_.size() < 4) return computePercentileInPlace(25.0);
        return computePercentileInPlace(25.0);
    }

    double getQ3() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (data_.size() < 4) return computePercentileInPlace(75.0);
        return computePercentileInPlace(75.0);
    }

    double getIQR() const {
        return getQ3() - getQ1();
    }

    bool isOutlier(double value) const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (data_.size() < 4) return false;
        double q1 = computePercentileInPlace(25.0);
        double q3 = computePercentileInPlace(75.0);
        double iqr = q3 - q1;
        double lower = q1 - 1.5 * iqr;
        double upper = q3 + 1.5 * iqr;
        return value < lower || value > upper;
    }

    size_t getCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return data_.size();
    }

private:
    OutlierDetector() = default;
    ~OutlierDetector() = default;

    OutlierDetector(const OutlierDetector&) = delete;
    OutlierDetector& operator=(const OutlierDetector&) = delete;
    OutlierDetector(OutlierDetector&&) = delete;
    OutlierDetector& operator=(OutlierDetector&&) = delete;

    void ensureSorted() const {
        if (!sorted_) {
            std::sort(data_.begin(), data_.end());
            sorted_ = true;
        }
    }

    double computePercentileInPlace(double p) const {
        ensureSorted();
        if (data_.empty()) return 0.0;
        if (p <= 0.0) return data_.front();
        if (p >= 100.0) return data_.back();
        double idx = (p / 100.0) * static_cast<double>(data_.size() - 1);
        size_t lo = static_cast<size_t>(idx);
        size_t hi = lo + 1;
        if (hi >= data_.size()) return data_[lo];
        double frac = idx - static_cast<double>(lo);
        return data_[lo] * (1.0 - frac) + data_[hi] * frac;
    }

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    mutable std::vector<double> data_;
    mutable bool sorted_ = true;
};

} // namespace nexus::utility::statistics
