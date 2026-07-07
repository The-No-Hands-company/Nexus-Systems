#pragma once

#include <string>
#include <vector>
#include <functional>
#include <source_location>
#include <map>
#include <algorithm>
#include <cmath>
#include <mutex>

namespace nexus::utility::statistics {

struct ProfileStats {
    double min = 0.0;
    double max = 0.0;
    double mean = 0.0;
    double median = 0.0;
    double p95 = 0.0;
    double p99 = 0.0;
    size_t count = 0;
};

class StatisticalProfiler {
public:
    static StatisticalProfiler& instance() {
        static StatisticalProfiler inst;
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
    }

    void recordTiming(const std::string& name, double ms) {
        std::lock_guard<std::mutex> lock(mutex_);
        data_[name].push_back(ms);
    }

    ProfileStats getStats(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        ProfileStats stats;
        auto it = data_.find(name);
        if (it == data_.end() || it->second.empty()) return stats;

        auto sorted = it->second;
        std::sort(sorted.begin(), sorted.end());
        stats.count = sorted.size();
        stats.min = sorted.front();
        stats.max = sorted.back();
        stats.mean = computeMean(sorted);
        stats.median = computePercentile(sorted, 50.0);
        stats.p95 = computePercentile(sorted, 95.0);
        stats.p99 = computePercentile(sorted, 99.0);
        return stats;
    }

    std::map<std::string, ProfileStats> getAllStats() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::map<std::string, ProfileStats> result;
        for (const auto& [name, _] : data_) {
            result[name] = getStatsUnsafe(name);
        }
        return result;
    }

    size_t getEntryCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return data_.size();
    }

private:
    StatisticalProfiler() = default;
    ~StatisticalProfiler() = default;

    StatisticalProfiler(const StatisticalProfiler&) = delete;
    StatisticalProfiler& operator=(const StatisticalProfiler&) = delete;
    StatisticalProfiler(StatisticalProfiler&&) = delete;
    StatisticalProfiler& operator=(StatisticalProfiler&&) = delete;

    double computeMean(const std::vector<double>& sorted) const {
        double sum = 0.0;
        for (double v : sorted) sum += v;
        return sum / static_cast<double>(sorted.size());
    }

    double computePercentile(const std::vector<double>& sorted, double p) const {
        if (sorted.empty()) return 0.0;
        if (p <= 0.0) return sorted.front();
        if (p >= 100.0) return sorted.back();
        double idx = (p / 100.0) * static_cast<double>(sorted.size() - 1);
        size_t lo = static_cast<size_t>(idx);
        size_t hi = lo + 1;
        if (hi >= sorted.size()) return sorted[lo];
        double frac = idx - static_cast<double>(lo);
        return sorted[lo] * (1.0 - frac) + sorted[hi] * frac;
    }

    ProfileStats getStatsUnsafe(const std::string& name) const {
        ProfileStats stats;
        auto it = data_.find(name);
        if (it == data_.end() || it->second.empty()) return stats;

        auto sorted = it->second;
        std::sort(sorted.begin(), sorted.end());
        stats.count = sorted.size();
        stats.min = sorted.front();
        stats.max = sorted.back();
        stats.mean = computeMean(sorted);
        stats.median = computePercentile(sorted, 50.0);
        stats.p95 = computePercentile(sorted, 95.0);
        stats.p99 = computePercentile(sorted, 99.0);
        return stats;
    }

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::map<std::string, std::vector<double>> data_;
};

} // namespace nexus::utility::statistics
