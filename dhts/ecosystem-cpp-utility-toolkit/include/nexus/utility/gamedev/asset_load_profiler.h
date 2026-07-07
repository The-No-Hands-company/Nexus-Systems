#pragma once

#include <string>
#include <vector>
#include <functional>
#include <source_location>
#include <map>
#include <algorithm>
#include <mutex>

namespace nexus::utility::gamedev {

struct AssetLoadRecord {
    std::string asset;
    double load_time_ms = 0.0;
    size_t bytes = 0;
};

struct AssetStats {
    double min_ms = 0.0;
    double max_ms = 0.0;
    double avg_ms = 0.0;
    size_t total_bytes = 0;
    size_t load_count = 0;
};

class AssetLoadProfiler {
public:
    static AssetLoadProfiler& instance() {
        static AssetLoadProfiler inst;
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
        records_.clear();
        total_load_time_ = 0.0;
        total_bytes_ = 0;
    }

    void recordLoad(const std::string& asset, double ms, size_t bytes) {
        std::lock_guard<std::mutex> lock(mutex_);
        records_[asset].push_back({asset, ms, bytes});
        total_load_time_ += ms;
        total_bytes_ += bytes;
    }

    AssetStats getStats(const std::string& asset) const {
        std::lock_guard<std::mutex> lock(mutex_);
        AssetStats stats;
        auto it = records_.find(asset);
        if (it == records_.end() || it->second.empty()) return stats;

        stats.load_count = it->second.size();
        double min_val = it->second[0].load_time_ms;
        double max_val = it->second[0].load_time_ms;
        double sum = 0.0;
        size_t total_b = 0;
        for (const auto& r : it->second) {
            if (r.load_time_ms < min_val) min_val = r.load_time_ms;
            if (r.load_time_ms > max_val) max_val = r.load_time_ms;
            sum += r.load_time_ms;
            total_b += r.bytes;
        }
        stats.min_ms = min_val;
        stats.max_ms = max_val;
        stats.avg_ms = sum / static_cast<double>(it->second.size());
        stats.total_bytes = total_b;
        return stats;
    }

    std::vector<AssetLoadRecord> getSlowestAssets(size_t n) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<AssetLoadRecord> all_records;
        for (const auto& [name, recs] : records_) {
            for (const auto& rec : recs) {
                all_records.push_back(rec);
            }
        }
        std::sort(all_records.begin(), all_records.end(),
            [](const AssetLoadRecord& a, const AssetLoadRecord& b) {
                return a.load_time_ms > b.load_time_ms;
            });
        if (all_records.size() > n) all_records.resize(n);
        return all_records;
    }

    double getTotalLoadTime() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return total_load_time_;
    }

    size_t getTotalBytes() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return total_bytes_;
    }

private:
    AssetLoadProfiler() = default;
    ~AssetLoadProfiler() = default;

    AssetLoadProfiler(const AssetLoadProfiler&) = delete;
    AssetLoadProfiler& operator=(const AssetLoadProfiler&) = delete;
    AssetLoadProfiler(AssetLoadProfiler&&) = delete;
    AssetLoadProfiler& operator=(AssetLoadProfiler&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::map<std::string, std::vector<AssetLoadRecord>> records_;
    double total_load_time_ = 0.0;
    size_t total_bytes_ = 0;
};

} // namespace nexus::utility::gamedev
