#pragma once

#include <cmath>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace nexus::utility::algorithmic {

class HashQualityAnalyzer {
public:
    static HashQualityAnalyzer& instance() {
        static HashQualityAnalyzer inst;
        return inst;
    }

    void initialize() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = true; }
    void shutdown() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = false; reset(); }
    bool isEnabled() const { return enabled_; }

    void addHash(uint64_t hash) {
        std::lock_guard<std::mutex> lock(mutex_);
        uint64_t bucket = hash % num_buckets_;
        ++buckets_[bucket];
        ++count_;
    }

    double getDistributionEntropy() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (count_ == 0) return 0.0;
        double entropy = 0.0;
        for (const auto& [_, bucket_count] : buckets_) {
            if (bucket_count > 0) {
                double p = static_cast<double>(bucket_count) / count_;
                entropy -= p * std::log2(p);
            }
        }
        return entropy;
    }

    double getCollisionRate() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (count_ == 0) return 0.0;
        uint64_t collisions = 0;
        for (const auto& [_, bucket_count] : buckets_) {
            if (bucket_count > 1) collisions += bucket_count - 1;
        }
        return static_cast<double>(collisions) / count_;
    }

    double getUniformityScore() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (count_ == 0) return 0.0;
        double expected = static_cast<double>(count_) / num_buckets_;
        double chi_sq = 0.0;
        for (size_t i = 0; i < num_buckets_; ++i) {
            auto it = buckets_.find(i);
            double observed = (it != buckets_.end()) ? static_cast<double>(it->second) : 0.0;
            double diff = observed - expected;
            chi_sq += (diff * diff) / expected;
        }
        return 1.0 / (1.0 + chi_sq / count_);
    }

    uint64_t getCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return count_;
    }

private:
    void reset() {
        buckets_.clear();
        count_ = 0;
    }

    HashQualityAnalyzer() = default;
    ~HashQualityAnalyzer() = default;
    HashQualityAnalyzer(const HashQualityAnalyzer&) = delete;
    HashQualityAnalyzer& operator=(const HashQualityAnalyzer&) = delete;

    static constexpr size_t num_buckets_ = 256;
    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::map<uint64_t, uint64_t> buckets_;
    uint64_t count_ = 0;
};

} // namespace nexus::utility::algorithmic
