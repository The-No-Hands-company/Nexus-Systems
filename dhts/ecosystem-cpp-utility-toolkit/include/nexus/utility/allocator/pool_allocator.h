#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include <memory>
#include <type_traits>
#include <new>
#include <stdexcept>
#include <mutex>

namespace nexus::utility::allocator {

/// @brief Fixed-size memory pool allocator. Pre-allocates N objects of size T,
/// recycles freed objects via an intrusive free list. O(1) allocate/deallocate.
///
/// Thread safety: All operations are protected by a mutex.
///
/// Usage:
///   PoolAllocator<MyClass> pool(1000);  // Pre-allocate 1000 MyClass objects
///   MyClass* obj = pool.allocate();     // Get a pooled object
///   pool.construct(obj, args...);       // Construct in-place
///   pool.destroy(obj);                  // Destroy
///   pool.deallocate(obj);               // Return to pool
template <typename T>
class PoolAllocator {
    static_assert(sizeof(T) >= sizeof(void*),
                  "Pooled type must be at least pointer-sized for free list");

public:
    using value_type = T;
    using size_type = size_t;
    using pointer = T*;
    using const_pointer = const T*;

    /// @brief Pre-allocate a pool of `count` objects
    explicit PoolAllocator(size_t count) : capacity_(count) {
        if (count == 0) {
            throw std::invalid_argument("PoolAllocator: count must be > 0");
        }
        // Allocate raw storage for all objects at once
        storage_ = static_cast<T*>(::operator new(count * sizeof(T), std::align_val_t{alignof(T)}));
        // Build free list
        for (size_t i = 0; i < count; ++i) {
            free_list_push(&storage_[i]);
        }
    }

    ~PoolAllocator() {
        if (storage_) {
            ::operator delete(storage_, std::align_val_t{alignof(T)});
        }
    }

    PoolAllocator(const PoolAllocator&) = delete;
    PoolAllocator& operator=(const PoolAllocator&) = delete;
    PoolAllocator(PoolAllocator&& other) noexcept
        : storage_(other.storage_)
        , capacity_(other.capacity_)
        , allocated_(other.allocated_.load())
        , free_list_(other.free_list_) {
        other.storage_ = nullptr;
        other.capacity_ = 0;
        other.allocated_.store(0);
        other.free_list_ = nullptr;
    }

    // ── Allocation ────────────────────────────────────────────────────────

    /// @brief Allocate a raw object from the pool. Returns nullptr if exhausted.
    [[nodiscard]] T* allocate() noexcept {
        std::lock_guard lock(mutex_);
        if (!free_list_) return nullptr;
        T* ptr = free_list_;
        free_list_ = *reinterpret_cast<T**>(free_list_);
        allocated_.fetch_add(1, std::memory_order_release);
        return ptr;
    }

    /// @brief Deallocate an object back to the pool
    void deallocate(T* ptr) noexcept {
        if (!ptr) return;
        std::lock_guard lock(mutex_);
        free_list_push(ptr);
        allocated_.fetch_sub(1, std::memory_order_release);
    }

    /// @brief Construct an object in pooled memory
    template <typename... Args>
    [[nodiscard]] T* construct(Args&&... args) {
        T* ptr = allocate();
        if (!ptr) return nullptr;
        try {
            ::new (static_cast<void*>(ptr)) T(std::forward<Args>(args)...);
        } catch (...) {
            deallocate(ptr);
            throw;
        }
        return ptr;
    }

    /// @brief Destroy and deallocate an object
    void destroy(T* ptr) noexcept(std::is_nothrow_destructible_v<T>) {
        if (!ptr) return;
        ptr->~T();
        deallocate(ptr);
    }

    // ── Query ─────────────────────────────────────────────────────────────

    [[nodiscard]] size_t capacity() const noexcept { return capacity_; }
    [[nodiscard]] size_t allocated() const noexcept {
        return allocated_.load(std::memory_order_acquire);
    }
    [[nodiscard]] size_t available() const noexcept {
        return capacity_ - allocated_.load(std::memory_order_acquire);
    }
    [[nodiscard]] bool empty() const noexcept {
        return allocated_.load(std::memory_order_acquire) == 0;
    }
    [[nodiscard]] bool full() const noexcept {
        return allocated_.load(std::memory_order_acquire) >= capacity_;
    }
    [[nodiscard]] double utilization() const noexcept {
        return capacity_ > 0
                   ? static_cast<double>(allocated_.load(std::memory_order_acquire)) /
                         static_cast<double>(capacity_)
                   : 0.0;
    }

private:
    void free_list_push(T* ptr) noexcept {
        *reinterpret_cast<T**>(ptr) = free_list_;
        free_list_ = ptr;
    }

    T* storage_ = nullptr;
    size_t capacity_ = 0;
    std::atomic<size_t> allocated_{0};
    T* free_list_ = nullptr;
    mutable std::mutex mutex_;
};

/// @brief Thread-unsafe pool allocator for single-threaded use.
/// Faster than PoolAllocator because no mutex overhead.
template <typename T>
class UnsafePoolAllocator {
    static_assert(sizeof(T) >= sizeof(void*),
                  "Pooled type must be at least pointer-sized for free list");

public:
    explicit UnsafePoolAllocator(size_t count) : capacity_(count) {
        if (count == 0) {
            throw std::invalid_argument("UnsafePoolAllocator: count must be > 0");
        }
        storage_ = static_cast<T*>(::operator new(count * sizeof(T), std::align_val_t{alignof(T)}));
        for (size_t i = 0; i < count; ++i) {
            free_list_push(&storage_[i]);
        }
    }

    ~UnsafePoolAllocator() {
        if (storage_) {
            ::operator delete(storage_, std::align_val_t{alignof(T)});
        }
    }

    UnsafePoolAllocator(const UnsafePoolAllocator&) = delete;
    UnsafePoolAllocator& operator=(const UnsafePoolAllocator&) = delete;

    [[nodiscard]] T* allocate() noexcept {
        if (!free_list_) return nullptr;
        T* ptr = free_list_;
        free_list_ = *reinterpret_cast<T**>(free_list_);
        ++allocated_;
        return ptr;
    }

    void deallocate(T* ptr) noexcept {
        if (!ptr) return;
        free_list_push(ptr);
        --allocated_;
    }

    template <typename... Args>
    [[nodiscard]] T* construct(Args&&... args) {
        T* ptr = allocate();
        if (!ptr) return nullptr;
        try {
            ::new (static_cast<void*>(ptr)) T(std::forward<Args>(args)...);
        } catch (...) {
            deallocate(ptr);
            throw;
        }
        return ptr;
    }

    void destroy(T* ptr) noexcept(std::is_nothrow_destructible_v<T>) {
        if (!ptr) return;
        ptr->~T();
        deallocate(ptr);
    }

    [[nodiscard]] size_t capacity() const noexcept { return capacity_; }
    [[nodiscard]] size_t allocated() const noexcept { return allocated_; }
    [[nodiscard]] size_t available() const noexcept { return capacity_ - allocated_; }

private:
    void free_list_push(T* ptr) noexcept {
        *reinterpret_cast<T**>(ptr) = free_list_;
        free_list_ = ptr;
    }

    T* storage_ = nullptr;
    size_t capacity_ = 0;
    size_t allocated_ = 0;
    T* free_list_ = nullptr;
};

} // namespace nexus::utility::allocator
