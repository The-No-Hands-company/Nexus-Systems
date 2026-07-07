#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <numeric>

namespace nexus::utility::cpp26 {

class HiveDebugger {
public:
    struct BucketStats {
        size_t bucketIndex;
        size_t elementCount;
        size_t activeElements;
        size_t erasedSlots;
        double utilization;
    };

    struct HiveReport {
        size_t totalBuckets;
        size_t totalElements;
        size_t totalErased;
        size_t maxBucketSize;
        double averageUtilization;
        std::vector<BucketStats> buckets;
    };

    void recordBucket(size_t index, size_t count, size_t active, size_t erased) {
        auto& bs = buckets_[index];
        bs.bucketIndex = index;
        bs.elementCount = count;
        bs.activeElements = active;
        bs.erasedSlots = erased;
        bs.utilization = count > 0 ? (double)active / count : 1.0;
    }

    HiveReport generateReport() const {
        HiveReport r;
        r.totalBuckets = buckets_.size();
        r.totalElements = 0;
        r.totalErased = 0;
        r.maxBucketSize = 0;
        r.averageUtilization = 0.0;

        for (const auto& [idx, bs] : buckets_) {
            r.totalElements += bs.activeElements;
            r.totalErased += bs.erasedSlots;
            r.maxBucketSize = std::max(r.maxBucketSize, bs.elementCount);
            r.buckets.push_back(bs);
        }
        if (!buckets_.empty()) {
            double sum = 0;
            for (const auto& [idx, bs] : buckets_) sum += bs.utilization;
            r.averageUtilization = sum / buckets_.size();
        }
        return r;
    }

    size_t wastedSlots() const {
        size_t w = 0;
        for (const auto& [idx, bs] : buckets_) w += bs.erasedSlots;
        return w;
    }

    void clear() { buckets_.clear(); }

private:
    std::unordered_map<size_t, BucketStats> buckets_;
};

} // namespace nexus::utility::cpp26
