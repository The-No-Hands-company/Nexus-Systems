#pragma once

#include <string>
#include <map>
#include <mutex>
#include <cstddef>

namespace nexus::utility::allocator {

/**
 * @brief Debug slab allocator behaviour by tracking per-slab allocations.
 *
 * A slab allocator carves fixed-size objects from larger slabs. This debugger
 * records allocations against each slab index (count and total bytes) and
 * reports per-slab statistics and utilization relative to a configured slab
 * capacity.
 *
 * Thread safety: all public methods are protected by a mutex.
 *
 * @category Allocator
 * @version 1.0.0
 */
class SlabAllocatorDebugger {
public:
    struct SlabStats {
        std::size_t alloc_count = 0;
        std::size_t total_bytes = 0;
    };

    /// @brief Get singleton instance
    static SlabAllocatorDebugger& instance() {
        static SlabAllocatorDebugger inst;
        return inst;
    }

    /// @brief Initialize the utility
    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = config;
        enabled_ = true;
        slabs_.clear();
    }

    /// @brief Shutdown the utility and cleanup resources
    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
        slabs_.clear();
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

    /// @brief Set the capacity (in bytes) of each slab, used for utilization.
    void setSlabCapacity(std::size_t capacity_bytes) {
        std::lock_guard<std::mutex> lock(mutex_);
        slab_capacity_ = capacity_bytes;
    }

    // ── Domain methods ──────────────────────────────────────────────────────

    /// @brief Record an allocation of `size` bytes from the given slab index.
    void recordSlabAlloc(std::size_t slab, std::size_t size) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!enabled_) return;
        auto& s = slabs_[slab];
        ++s.alloc_count;
        s.total_bytes += size;
    }

    /// @brief Get the statistics for a slab (allocation count and total bytes).
    SlabStats getSlabStats(std::size_t slab) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = slabs_.find(slab);
        if (it == slabs_.end()) return {};
        return it->second;
    }

    /// @brief Number of distinct slabs that have been used.
    std::size_t getTotalSlabs() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return slabs_.size();
    }

    /// @brief Utilization of a slab: total allocated bytes divided by slab
    /// capacity. Returns a value in [0.0, 1.0+] (may exceed 1.0 if the slab is
    /// oversubscribed relative to the configured capacity). Returns 0.0 if no
    /// capacity has been configured.
    double getSlabUtilization(std::size_t slab) const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (slab_capacity_ == 0) return 0.0;
        auto it = slabs_.find(slab);
        if (it == slabs_.end()) return 0.0;
        return static_cast<double>(it->second.total_bytes) /
               static_cast<double>(slab_capacity_);
    }

    /// @brief Total bytes allocated across all slabs.
    std::size_t getTotalBytes() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::size_t total = 0;
        for (const auto& [slab, s] : slabs_) {
            total += s.total_bytes;
        }
        return total;
    }

    /// @brief Get utility statistics/status
    std::string getStatus() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::size_t total = 0;
        for (const auto& [slab, s] : slabs_) {
            total += s.total_bytes;
        }
        return "SlabAllocatorDebugger[slabs=" + std::to_string(slabs_.size()) +
               ", total_bytes=" + std::to_string(total) + "]";
    }

    /// @brief Reset utility state
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        slabs_.clear();
    }

private:
    SlabAllocatorDebugger() = default;
    ~SlabAllocatorDebugger() = default;

    SlabAllocatorDebugger(const SlabAllocatorDebugger&) = delete;
    SlabAllocatorDebugger& operator=(const SlabAllocatorDebugger&) = delete;
    SlabAllocatorDebugger(SlabAllocatorDebugger&&) = delete;
    SlabAllocatorDebugger& operator=(SlabAllocatorDebugger&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::map<std::size_t, SlabStats> slabs_;
    std::size_t slab_capacity_ = 0;
};

} // namespace nexus::utility::allocator
