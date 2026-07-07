#pragma once

#include <string>
#include <mutex>
#include <cstddef>

namespace nexus::utility::allocator {

/**
 * @brief Track cumulative allocator statistics: total allocated/freed bytes,
 * current live usage, and peak usage.
 *
 * Thread safety: all public methods are protected by a mutex.
 *
 * @category Allocator
 * @version 1.0.0
 */
class AllocatorStatistics {
public:
    /// @brief Get singleton instance
    static AllocatorStatistics& instance() {
        static AllocatorStatistics inst;
        return inst;
    }

    /// @brief Initialize the utility
    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = config;
        enabled_ = true;
        total_allocated_ = 0;
        total_freed_ = 0;
        current_usage_ = 0;
        peak_usage_ = 0;
        alloc_count_ = 0;
        free_count_ = 0;
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

    /// @brief Record an allocation of `size` bytes.
    void recordAlloc(std::size_t size) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!enabled_) return;
        total_allocated_ += size;
        current_usage_ += size;
        ++alloc_count_;
        if (current_usage_ > peak_usage_) {
            peak_usage_ = current_usage_;
        }
    }

    /// @brief Record a free of `size` bytes.
    void recordFree(std::size_t size) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!enabled_) return;
        total_freed_ += size;
        current_usage_ = (current_usage_ >= size) ? current_usage_ - size : 0;
        ++free_count_;
    }

    /// @brief Total bytes ever allocated.
    std::size_t getTotalAllocated() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return total_allocated_;
    }

    /// @brief Total bytes ever freed.
    std::size_t getTotalFreed() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return total_freed_;
    }

    /// @brief Current live usage (allocated minus freed).
    std::size_t getCurrentUsage() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return current_usage_;
    }

    /// @brief Peak live usage observed.
    std::size_t getPeakUsage() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return peak_usage_;
    }

    /// @brief Number of allocation events recorded.
    std::size_t getAllocCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return alloc_count_;
    }

    /// @brief Number of free events recorded.
    std::size_t getFreeCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return free_count_;
    }

    /// @brief Outstanding allocations (alloc count minus free count).
    std::size_t getOutstandingCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return (alloc_count_ >= free_count_) ? alloc_count_ - free_count_ : 0;
    }

    /// @brief Get utility statistics/status
    std::string getStatus() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return "AllocatorStatistics[allocated=" + std::to_string(total_allocated_) +
               ", freed=" + std::to_string(total_freed_) +
               ", current=" + std::to_string(current_usage_) +
               ", peak=" + std::to_string(peak_usage_) + "]";
    }

    /// @brief Reset utility state
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        total_allocated_ = 0;
        total_freed_ = 0;
        current_usage_ = 0;
        peak_usage_ = 0;
        alloc_count_ = 0;
        free_count_ = 0;
    }

private:
    AllocatorStatistics() = default;
    ~AllocatorStatistics() = default;

    AllocatorStatistics(const AllocatorStatistics&) = delete;
    AllocatorStatistics& operator=(const AllocatorStatistics&) = delete;
    AllocatorStatistics(AllocatorStatistics&&) = delete;
    AllocatorStatistics& operator=(AllocatorStatistics&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::size_t total_allocated_ = 0;
    std::size_t total_freed_ = 0;
    std::size_t current_usage_ = 0;
    std::size_t peak_usage_ = 0;
    std::size_t alloc_count_ = 0;
    std::size_t free_count_ = 0;
};

} // namespace nexus::utility::allocator
