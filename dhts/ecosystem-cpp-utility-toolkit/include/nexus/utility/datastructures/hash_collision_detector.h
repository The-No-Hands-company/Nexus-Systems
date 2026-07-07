#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <string>
#include <mutex>

namespace nexus::utility::datastructures {

/// @brief Tracks hash collisions to detect poor hash function performance
class HashCollisionDetector {
public:
    struct Stats {
        size_t total_insertions = 0;
        size_t total_collisions = 0;
        size_t peak_bucket_size = 0;
        size_t empty_buckets = 0;
        size_t bucket_count = 0;
        double average_bucket_size = 0.0;
        double collision_rate = 0.0;  // Collisions per insertion
        std::vector<size_t> bucket_sizes;
    };

    HashCollisionDetector() = default;

    explicit HashCollisionDetector(size_t expected_buckets) {
        configure(expected_buckets);
    }

    void configure(size_t expected_buckets) {
        std::lock_guard lock(mutex_);
        bucket_count_ = expected_buckets > 0 ? expected_buckets : 1;
        buckets_.resize(bucket_count_, 0);
        total_insertions_ = 0;
        total_collisions_ = 0;
        peak_bucket_size_ = 0;
    }

    /// @brief Record a hash insertion. Hash value determines the bucket.
    void recordInsertion(uint64_t hash) {
        std::lock_guard lock(mutex_);
        size_t bucket = hash % bucket_count_;

        if (buckets_[bucket] > 0) {
            ++total_collisions_;
        }
        ++buckets_[bucket];
        ++total_insertions_;

        if (buckets_[bucket] > peak_bucket_size_) {
            peak_bucket_size_ = buckets_[bucket];
        }
    }

    /// @brief Record a key removal (decrements the bucket count)
    void recordRemoval(uint64_t hash) {
        std::lock_guard lock(mutex_);
        size_t bucket = hash % bucket_count_;
        if (buckets_[bucket] > 0) {
            --buckets_[bucket];
        }
    }

    [[nodiscard]] Stats getStats() const {
        std::lock_guard lock(mutex_);

        Stats s;
        s.total_insertions = total_insertions_;
        s.total_collisions = total_collisions_;
        s.peak_bucket_size = peak_bucket_size_;
        s.bucket_count = bucket_count_;

        // Count empty buckets and compute average
        for (size_t count : buckets_) {
            if (count == 0) ++s.empty_buckets;
        }
        s.average_bucket_size = total_insertions_ > 0
                                    ? static_cast<double>(total_insertions_) / static_cast<double>(bucket_count_)
                                    : 0.0;
        s.collision_rate = total_insertions_ > 0
                               ? static_cast<double>(total_collisions_) / static_cast<double>(total_insertions_)
                               : 0.0;
        s.bucket_sizes = buckets_;
        return s;
    }

    void reset() noexcept {
        std::lock_guard lock(mutex_);
        std::fill(buckets_.begin(), buckets_.end(), 0);
        total_insertions_ = 0;
        total_collisions_ = 0;
        peak_bucket_size_ = 0;
    }

private:
    mutable std::mutex mutex_;
    std::vector<size_t> buckets_;
    size_t bucket_count_ = 1;
    size_t total_insertions_ = 0;
    size_t total_collisions_ = 0;
    size_t peak_bucket_size_ = 0;
};

} // namespace nexus::utility::datastructures
