#pragma once

#include <atomic>
#include <stdexcept>
#include <string>

namespace nexus::utility::memory {

/**
 * @brief RAII guard for allocation limits and tracking.
 */
class AllocationGuard {
public:
    /**
     * @brief Sets global allocation limit.
     */
    static void setGlobalLimit(size_t bytes) {
        global_limit_.store(bytes, std::memory_order_release);
    }

    /**
     * @brief Gets current allocation count.
     */
    static size_t getCurrentAllocations() {
        return current_allocations_.load(std::memory_order_acquire);
    }

    /**
     * @brief Gets peak allocations.
     */
    static size_t getPeakAllocations() {
        return peak_allocations_.load(std::memory_order_acquire);
    }

    /**
     * @brief Records an allocation.
     */
    static void recordAllocation(size_t bytes) {
        size_t current = current_allocations_.fetch_add(bytes, std::memory_order_acq_rel) + bytes;
        
        // Update peak
        size_t peak = peak_allocations_.load(std::memory_order_acquire);
        while (current > peak) {
            if (peak_allocations_.compare_exchange_weak(peak, current, std::memory_order_release)) {
                break;
            }
        }
        
        // Check limit
        size_t limit = global_limit_.load(std::memory_order_acquire);
        if (limit > 0 && current > limit) {
            throw std::bad_alloc();
        }
    }

    /**
     * @brief Records a deallocation.
     */
    static void recordDeallocation(size_t bytes) {
        current_allocations_.fetch_sub(bytes, std::memory_order_acq_rel);
    }

    /**
     * @brief RAII scoped allocation limit.
     */
    class ScopedLimit {
    public:
        explicit ScopedLimit(size_t bytes) : previous_limit_(global_limit_.load()) {
            setGlobalLimit(bytes);
        }

        ~ScopedLimit() {
            setGlobalLimit(previous_limit_);
        }

    private:
        size_t previous_limit_;
    };

    /**
     * @brief Prevents allocations in scope.
     */
    class ScopedNoAlloc {
    public:
        ScopedNoAlloc() : previous_limit_(global_limit_.load()) {
            setGlobalLimit(current_allocations_.load());
        }

        ~ScopedNoAlloc() {
            setGlobalLimit(previous_limit_);
        }

    private:
        size_t previous_limit_;
    };

    /**
     * @brief Resets statistics.
     */
    static void reset() {
        current_allocations_.store(0, std::memory_order_release);
        peak_allocations_.store(0, std::memory_order_release);
        global_limit_.store(0, std::memory_order_release);
    }

private:
    static inline std::atomic<size_t> current_allocations_{0};
    static inline std::atomic<size_t> peak_allocations_{0};
    static inline std::atomic<size_t> global_limit_{0};
};

} // namespace nexus::utility::memory
