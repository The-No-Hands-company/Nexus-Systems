#pragma once

#include <string>
#include <mutex>
#include <cstddef>

namespace nexus::utility::allocator {

/**
 * @brief Profile garbage collection cycles: timing and reclaimed memory.
 *
 * Records each collection's duration (in milliseconds) and the number of bytes
 * reclaimed, then exposes aggregate metrics such as average collection time and
 * total memory freed across all cycles.
 *
 * Thread safety: all public methods are protected by a mutex.
 *
 * @category Allocator
 * @version 1.0.0
 */
class GarbageCollectorProfiler {
public:
    /// @brief Get singleton instance
    static GarbageCollectorProfiler& instance() {
        static GarbageCollectorProfiler inst;
        return inst;
    }

    /// @brief Initialize the utility
    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = config;
        enabled_ = true;
        collection_count_ = 0;
        total_time_ms_ = 0.0;
        total_freed_ = 0;
        max_time_ms_ = 0.0;
    }

    /// @brief Shutdown the utility and cleanup resources
    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
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

    /// @brief Record a completed collection cycle.
    /// @param ms Duration of the collection in milliseconds.
    /// @param freed Number of bytes reclaimed by the collection.
    void recordCollection(double ms, std::size_t freed) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!enabled_) return;
        ++collection_count_;
        total_time_ms_ += ms;
        total_freed_ += freed;
        if (ms > max_time_ms_) max_time_ms_ = ms;
    }

    /// @brief Average collection time across all recorded cycles (ms).
    double getAvgCollectionTime() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (collection_count_ == 0) return 0.0;
        return total_time_ms_ / static_cast<double>(collection_count_);
    }

    /// @brief Total bytes freed across all collections.
    std::size_t getTotalFreed() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return total_freed_;
    }

    /// @brief Number of collection cycles recorded.
    std::size_t getCollectionCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return collection_count_;
    }

    /// @brief Total time spent collecting (ms).
    double getTotalCollectionTime() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return total_time_ms_;
    }

    /// @brief Worst-case (maximum) collection pause (ms).
    double getMaxCollectionTime() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return max_time_ms_;
    }

    /// @brief Get utility statistics/status
    std::string getStatus() const {
        std::lock_guard<std::mutex> lock(mutex_);
        double avg = collection_count_ == 0 ? 0.0
                     : total_time_ms_ / static_cast<double>(collection_count_);
        return "GarbageCollectorProfiler[collections=" + std::to_string(collection_count_) +
               ", avg_ms=" + std::to_string(avg) +
               ", total_freed=" + std::to_string(total_freed_) + "]";
    }

    /// @brief Reset utility state
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        collection_count_ = 0;
        total_time_ms_ = 0.0;
        total_freed_ = 0;
        max_time_ms_ = 0.0;
    }

private:
    GarbageCollectorProfiler() = default;
    ~GarbageCollectorProfiler() = default;

    GarbageCollectorProfiler(const GarbageCollectorProfiler&) = delete;
    GarbageCollectorProfiler& operator=(const GarbageCollectorProfiler&) = delete;
    GarbageCollectorProfiler(GarbageCollectorProfiler&&) = delete;
    GarbageCollectorProfiler& operator=(GarbageCollectorProfiler&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::size_t collection_count_ = 0;
    double total_time_ms_ = 0.0;
    std::size_t total_freed_ = 0;
    double max_time_ms_ = 0.0;
};

} // namespace nexus::utility::allocator
