#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <source_location>
#include <ostream>

namespace nexus::utility::memory {

/**
 * @brief Arena allocator with bounds checking and leak detection
 * 
 * Provides a memory pool with debug features:
 * - Bounds checking with canaries
 * - Leak detection on pool destruction
 * - Allocation tracking with source location
 * - Double-free detection
 * - Use-after-free detection (poison freed memory)
 */
class MemoryPoolDebug {
public:
    struct PoolConfig {
        size_t pool_size = 1024 * 1024;  // 1MB default
        size_t alignment = 16;
        bool enable_canaries = true;
        bool enable_leak_detection = true;
        bool poison_freed_memory = true;
        uint8_t poison_byte = 0xDD;
        std::string pool_name = "UnnamedPool";
    };

    struct AllocationInfo {
        void* ptr;
        size_t size;
        std::source_location location;
        bool is_freed = false;
    };

    explicit MemoryPoolDebug(const PoolConfig& config);
    ~MemoryPoolDebug();

    // Non-copyable, non-movable
    MemoryPoolDebug(const MemoryPoolDebug&) = delete;
    MemoryPoolDebug& operator=(const MemoryPoolDebug&) = delete;

    /**
     * @brief Allocate memory from the pool with debug tracking
     */
    void* allocate(size_t size, size_t alignment = 16,
                   const std::source_location& loc = std::source_location::current());

    /**
     * @brief Deallocate memory with validation
     */
    void deallocate(void* ptr,
                    const std::source_location& loc = std::source_location::current());

    /**
     * @brief Check for memory leaks
     */
    std::vector<AllocationInfo> detectLeaks() const;

    /**
     * @brief Validate pool integrity (check canaries)
     */
    bool validateIntegrity() const;

    /**
     * @brief Reset the pool (frees all allocations)
     */
    void reset();

    /**
     * @brief Get allocation statistics
     */
    struct Statistics {
        size_t total_allocated;
        size_t total_freed;
        size_t current_allocations;
        size_t peak_usage;
        size_t failed_allocations;
    };
    Statistics getStatistics() const;

    /**
     * @brief Print detailed allocation report
     */
    void printReport(std::ostream& os) const;

private:
    static constexpr uint32_t CANARY_VALUE = 0xDEADBEEF;
    
    struct CanaryBlock {
        uint32_t front_canary;
        // User data here
        // uint32_t back_canary (at end)
    };

    PoolConfig config_;
    std::unique_ptr<uint8_t[]> pool_memory_;
    size_t current_offset_ = 0;
    
    mutable std::mutex mutex_;
    std::unordered_map<void*, AllocationInfo> allocations_;
    
    Statistics stats_{};

    void* placeCanaries(void* ptr, size_t size);
    bool checkCanaries(void* ptr) const;
    void poisonMemory(void* ptr, size_t size);
};

/**
 * @brief RAII wrapper for pool allocations
 */
template<typename T>
class PoolAllocated {
public:
    PoolAllocated(MemoryPoolDebug& pool, const std::source_location& loc = std::source_location::current())
        : pool_(pool), ptr_(static_cast<T*>(pool.allocate(sizeof(T), alignof(T), loc))) {
        new (ptr_) T();
    }

    template<typename... Args>
    PoolAllocated(MemoryPoolDebug& pool, Args&&... args,
                  const std::source_location& loc = std::source_location::current())
        : pool_(pool), ptr_(static_cast<T*>(pool.allocate(sizeof(T), alignof(T), loc))) {
        new (ptr_) T(std::forward<Args>(args)...);
    }

    ~PoolAllocated() {
        if (ptr_) {
            ptr_->~T();
            pool_.deallocate(ptr_);
        }
    }

    T* get() { return ptr_; }
    const T* get() const { return ptr_; }
    T& operator*() { return *ptr_; }
    const T& operator*() const { return *ptr_; }
    T* operator->() { return ptr_; }
    const T* operator->() const { return ptr_; }

private:
    MemoryPoolDebug& pool_;
    T* ptr_;
};

} // namespace nexus::utility::memory
