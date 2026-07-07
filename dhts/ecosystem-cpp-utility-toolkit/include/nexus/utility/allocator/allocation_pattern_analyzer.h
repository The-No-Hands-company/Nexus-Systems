#pragma once

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <algorithm>
#include <cstddef>

namespace nexus::utility::allocator {

/**
 * @brief Analyze allocation patterns by tracking allocation sizes and alignments.
 *
 * Records every allocation size/alignment and builds a histogram that can be
 * queried to detect the most common allocation sizes. Useful for tuning pool
 * and slab allocators to the dominant object sizes in a workload.
 *
 * Thread safety: all public methods are protected by a mutex.
 *
 * @category Allocator
 * @version 1.0.0
 */
class AllocationPatternAnalyzer {
public:
    /// @brief Get singleton instance
    static AllocationPatternAnalyzer& instance() {
        static AllocationPatternAnalyzer inst;
        return inst;
    }

    /// @brief Initialize the utility
    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = config;
        enabled_ = true;
        size_histogram_.clear();
        alignment_histogram_.clear();
        total_allocations_ = 0;
    }

    /// @brief Shutdown the utility and cleanup resources
    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
        size_histogram_.clear();
        alignment_histogram_.clear();
        total_allocations_ = 0;
    }

    /// @brief Check if the utility is currently enabled
    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    /// @brief Enable the utility
    void enable() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = true;
    }

    /// @brief Disable the utility
    void disable() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
    }

    // ── Domain methods ──────────────────────────────────────────────────────

    /// @brief Record a single allocation of the given size and alignment.
    void recordAllocation(std::size_t size, std::size_t alignment = alignof(std::max_align_t)) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!enabled_) return;
        ++size_histogram_[size];
        ++alignment_histogram_[alignment];
        ++total_allocations_;
    }

    /// @brief Detect the most common allocation sizes, ordered by frequency (descending).
    /// @param top_n Maximum number of sizes to return (0 = all).
    /// @return Vector of {size, count} pairs sorted by descending count.
    std::vector<std::pair<std::size_t, std::size_t>> detectPatterns(std::size_t top_n = 0) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::pair<std::size_t, std::size_t>> patterns(
            size_histogram_.begin(), size_histogram_.end());
        std::sort(patterns.begin(), patterns.end(),
                  [](const auto& a, const auto& b) {
                      if (a.second != b.second) return a.second > b.second;
                      return a.first < b.first;
                  });
        if (top_n > 0 && patterns.size() > top_n) {
            patterns.resize(top_n);
        }
        return patterns;
    }

    /// @brief Get the raw size histogram (size -> occurrence count).
    std::map<std::size_t, std::size_t> getSizeHistogram() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return size_histogram_;
    }

    /// @brief Get the alignment histogram (alignment -> occurrence count).
    std::map<std::size_t, std::size_t> getAlignmentHistogram() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return alignment_histogram_;
    }

    /// @brief Total number of recorded allocations.
    std::size_t getTotalAllocations() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return total_allocations_;
    }

    /// @brief Number of distinct allocation sizes seen.
    std::size_t getDistinctSizeCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return size_histogram_.size();
    }

    /// @brief Get utility statistics/status
    std::string getStatus() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return "AllocationPatternAnalyzer[enabled=" + std::string(enabled_ ? "true" : "false") +
               ", allocations=" + std::to_string(total_allocations_) +
               ", distinct_sizes=" + std::to_string(size_histogram_.size()) + "]";
    }

    /// @brief Reset utility state
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        size_histogram_.clear();
        alignment_histogram_.clear();
        total_allocations_ = 0;
    }

private:
    AllocationPatternAnalyzer() = default;
    ~AllocationPatternAnalyzer() = default;

    AllocationPatternAnalyzer(const AllocationPatternAnalyzer&) = delete;
    AllocationPatternAnalyzer& operator=(const AllocationPatternAnalyzer&) = delete;
    AllocationPatternAnalyzer(AllocationPatternAnalyzer&&) = delete;
    AllocationPatternAnalyzer& operator=(AllocationPatternAnalyzer&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::map<std::size_t, std::size_t> size_histogram_;
    std::map<std::size_t, std::size_t> alignment_histogram_;
    std::size_t total_allocations_ = 0;
};

} // namespace nexus::utility::allocator
