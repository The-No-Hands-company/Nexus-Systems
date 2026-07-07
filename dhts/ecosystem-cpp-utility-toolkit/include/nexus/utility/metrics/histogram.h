#pragma once

#include <atomic>
#include <cstdint>
#include <vector>
#include <string>
#include <string_view>
#include <unordered_map>
#include <mutex>
#include <shared_mutex>
#include <algorithm>
#include <cmath>
#include <limits>
#include <array>

namespace nexus::utility::metrics {

class Histogram {
public:
    struct Snapshot {
        uint64_t count = 0;
        double sum = 0.0;
        double min = std::numeric_limits<double>::max();
        double max = std::numeric_limits<double>::lowest();
        double mean = 0.0;
        double p50 = 0.0;
        double p95 = 0.0;
        double p99 = 0.0;
        std::vector<uint64_t> bucket_counts;
        std::vector<double> bucket_bounds;
    };

    Histogram() = default;

    /// @brief Configure with exponential buckets from base^min_power to base^max_power
    Histogram& configureExponential(double base, int min_power, int max_power) {
        std::lock_guard lock(mutex_);
        bounds_.clear();
        buckets_.clear();
        for (int i = min_power; i <= max_power; ++i) {
            bounds_.push_back(std::pow(base, i));
        }
        buckets_.resize(bounds_.size() + 1, 0);
        return *this;
    }

    /// @brief Configure with linear buckets
    Histogram& configureLinear(double start, double width, size_t count) {
        std::lock_guard lock(mutex_);
        bounds_.clear();
        buckets_.clear();
        for (size_t i = 0; i < count; ++i) {
            bounds_.push_back(start + static_cast<double>(i) * width);
        }
        buckets_.resize(bounds_.size() + 1, 0);
        return *this;
    }

    /// @brief Configure with explicit bounds
    Histogram& configure(std::vector<double> bounds) {
        std::lock_guard lock(mutex_);
        bounds_ = std::move(bounds);
        buckets_.resize(bounds_.size() + 1, 0);
        return *this;
    }

    void observe(double value) noexcept {
        std::lock_guard lock(mutex_);

        if (bounds_.empty()) {
            configureExponential(2.0, 0, 32);
        }

        ++count_;
        sum_ += value;

        if (value < min_) min_ = value;
        if (value > max_) max_ = value;

        // Store value for percentile calculation (ring buffer of last N)
        values_[value_idx_ % kMaxValues] = value;
        ++value_idx_;

        // Find bucket
        auto it = std::upper_bound(bounds_.begin(), bounds_.end(), value);
        size_t idx = static_cast<size_t>(it - bounds_.begin());
        if (idx < buckets_.size()) {
            ++buckets_[idx];
        }
    }

    [[nodiscard]] Snapshot snapshot() const {
        std::lock_guard lock(mutex_);

        Snapshot s;
        s.count = count_;
        s.sum = sum_;
        s.min = count_ > 0 ? min_ : 0.0;
        s.max = count_ > 0 ? max_ : 0.0;
        s.mean = count_ > 0 ? sum_ / static_cast<double>(count_) : 0.0;

        // Calculate percentiles from stored values
        size_t n = std::min(count_, kMaxValues);
        if (n > 0) {
            std::vector<double> sorted(values_.begin(), values_.begin() + static_cast<ptrdiff_t>(n));
            std::sort(sorted.begin(), sorted.end());
            s.p50 = percentile(sorted, 0.50);
            s.p95 = percentile(sorted, 0.95);
            s.p99 = percentile(sorted, 0.99);
        }

        s.bucket_counts = buckets_;
        s.bucket_bounds = bounds_;
        return s;
    }

    [[nodiscard]] uint64_t count() const noexcept {
        std::lock_guard lock(mutex_);
        return count_;
    }

    void reset() noexcept {
        std::lock_guard lock(mutex_);
        count_ = 0;
        sum_ = 0.0;
        min_ = std::numeric_limits<double>::max();
        max_ = std::numeric_limits<double>::lowest();
        std::fill(buckets_.begin(), buckets_.end(), 0);
        std::fill(values_.begin(), values_.end(), 0.0);
        value_idx_ = 0;
    }

private:
    [[nodiscard]] static double percentile(const std::vector<double>& sorted, double p) noexcept {
        if (sorted.empty()) return 0.0;
        size_t idx = static_cast<size_t>(p * static_cast<double>(sorted.size() - 1));
        if (idx >= sorted.size()) idx = sorted.size() - 1;
        return sorted[idx];
    }

    static constexpr size_t kMaxValues = 10000;

    mutable std::mutex mutex_;
    uint64_t count_ = 0;
    double sum_ = 0.0;
    double min_ = std::numeric_limits<double>::max();
    double max_ = std::numeric_limits<double>::lowest();
    std::vector<double> bounds_;
    std::vector<uint64_t> buckets_;
    std::array<double, kMaxValues> values_{};
    size_t value_idx_ = 0;
};

class LabeledHistogram {
public:
    [[nodiscard]] static LabeledHistogram& instance() noexcept {
        static LabeledHistogram h;
        return h;
    }

    void configureExponential(std::string_view label, double base, int min_power, int max_power) {
        std::unique_lock lock(mutex_);
        histograms_[std::string(label)].configureExponential(base, min_power, max_power);
    }

    void observe(std::string_view label, double value) noexcept {
        std::shared_lock lock(mutex_);
        auto it = histograms_.find(std::string(label));
        if (it != histograms_.end()) {
            it->second.observe(value);
        }
    }

    [[nodiscard]] Histogram::Snapshot snapshot(std::string_view label) const {
        std::shared_lock lock(mutex_);
        auto it = histograms_.find(std::string(label));
        if (it != histograms_.end()) {
            return it->second.snapshot();
        }
        return {};
    }

    void reset() noexcept {
        std::unique_lock lock(mutex_);
        histograms_.clear();
    }

private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, Histogram> histograms_;
};

} // namespace nexus::utility::metrics
