#pragma once

#include <string>
#include <map>
#include <mutex>
#include <cstddef>

namespace nexus::utility::allocator {

/**
 * @brief Monitor per-slot usage of a pool allocator.
 *
 * Tracks how many times each slot has been allocated and freed, and reports the
 * current utilization of any slot (allocations minus frees). Useful for
 * detecting hot slots, double-frees, and leaked slots in a fixed-size pool.
 *
 * Thread safety: all public methods are protected by a mutex.
 *
 * @category Allocator
 * @version 1.0.0
 */
class PoolAllocatorMonitor {
public:
    struct SlotState {
        std::size_t allocs = 0;
        std::size_t frees = 0;
    };

    /// @brief Get singleton instance
    static PoolAllocatorMonitor& instance() {
        static PoolAllocatorMonitor inst;
        return inst;
    }

    /// @brief Initialize the utility
    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = config;
        enabled_ = true;
        slots_.clear();
    }

    /// @brief Shutdown the utility and cleanup resources
    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
        slots_.clear();
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

    /// @brief Record an allocation of the given slot index.
    void recordAlloc(std::size_t slot) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!enabled_) return;
        ++slots_[slot].allocs;
    }

    /// @brief Record a free of the given slot index.
    void recordFree(std::size_t slot) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!enabled_) return;
        ++slots_[slot].frees;
    }

    /// @brief Current utilization of a slot: allocations minus frees.
    /// A value > 0 means the slot is currently in use; a value below zero
    /// (clamped to 0) would indicate more frees than allocations.
    std::size_t getSlotUtilization(std::size_t slot) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = slots_.find(slot);
        if (it == slots_.end()) return 0;
        const auto& s = it->second;
        return (s.allocs >= s.frees) ? s.allocs - s.frees : 0;
    }

    /// @brief Detailed state for a slot (allocs and frees).
    SlotState getSlotState(std::size_t slot) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = slots_.find(slot);
        if (it == slots_.end()) return {};
        return it->second;
    }

    /// @brief Number of distinct slots that have been touched.
    std::size_t getTotalSlots() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return slots_.size();
    }

    /// @brief Number of slots currently allocated (utilization > 0).
    std::size_t getActiveSlots() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::size_t active = 0;
        for (const auto& [slot, s] : slots_) {
            if (s.allocs > s.frees) ++active;
        }
        return active;
    }

    /// @brief Get utility statistics/status
    std::string getStatus() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::size_t active = 0;
        for (const auto& [slot, s] : slots_) {
            if (s.allocs > s.frees) ++active;
        }
        return "PoolAllocatorMonitor[slots=" + std::to_string(slots_.size()) +
               ", active=" + std::to_string(active) + "]";
    }

    /// @brief Reset utility state
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        slots_.clear();
    }

private:
    PoolAllocatorMonitor() = default;
    ~PoolAllocatorMonitor() = default;

    PoolAllocatorMonitor(const PoolAllocatorMonitor&) = delete;
    PoolAllocatorMonitor& operator=(const PoolAllocatorMonitor&) = delete;
    PoolAllocatorMonitor(PoolAllocatorMonitor&&) = delete;
    PoolAllocatorMonitor& operator=(PoolAllocatorMonitor&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::map<std::size_t, SlotState> slots_;
};

} // namespace nexus::utility::allocator
