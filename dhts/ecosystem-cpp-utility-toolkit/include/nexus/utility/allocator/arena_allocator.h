#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include <memory>
#include <stdexcept>
#include <algorithm>
#include <new>

namespace nexus::utility::allocator {

/// @brief Bump-pointer arena allocator for fast temporary allocations.
/// All allocations come from a pre-allocated block. Use reset() to reclaim all memory at once.
/// NOT thread-safe. Designed for single-threaded temporary allocation patterns.
class ArenaAllocator {
public:
    /// @brief Create an arena with a pre-allocated block
    explicit ArenaAllocator(size_t block_size)
        : block_(std::make_unique<std::byte[]>(block_size))
        , block_size_(block_size)
        , offset_(0) {}

    /// @brief Allocate raw memory from the arena
    [[nodiscard]] void* allocate(size_t size, size_t alignment = alignof(std::max_align_t)) {
        uintptr_t base = reinterpret_cast<uintptr_t>(block_.get());
        uintptr_t aligned = (base + offset_ + alignment - 1) & ~(alignment - 1);
        size_t aligned_offset = aligned - base;

        if (aligned_offset + size > block_size_) {
            throw std::bad_alloc();
        }

        void* ptr = reinterpret_cast<void*>(aligned);
        offset_ = aligned_offset + size;
        ++allocation_count_;
        total_allocated_ += size;
        if (offset_ > peak_offset_) peak_offset_ = offset_;
        return ptr;
    }

    /// @brief Typed allocation
    template <typename T, typename... Args>
    [[nodiscard]] T* create(Args&&... args) {
        void* mem = allocate(sizeof(T), alignof(T));
        return new (mem) T(std::forward<Args>(args)...);
    }

    /// @brief Deallocate (no-op in arena - memory is reclaimed on reset)
    void deallocate(void*, size_t) noexcept {}

    /// @brief Reclaim all memory - resets the bump pointer to zero
    void reset() noexcept {
        offset_ = 0;
        allocation_count_ = 0;
        total_allocated_ = 0;
    }

    [[nodiscard]] size_t used() const noexcept { return offset_; }
    [[nodiscard]] size_t capacity() const noexcept { return block_size_; }
    [[nodiscard]] size_t remaining() const noexcept {
        return offset_ < block_size_ ? block_size_ - offset_ : 0;
    }
    [[nodiscard]] size_t allocationCount() const noexcept { return allocation_count_; }
    [[nodiscard]] size_t totalAllocated() const noexcept { return total_allocated_; }
    [[nodiscard]] size_t peakUsed() const noexcept { return peak_offset_; }
    [[nodiscard]] double utilization() const noexcept {
        return block_size_ > 0 ? static_cast<double>(peak_offset_) / static_cast<double>(block_size_) : 0.0;
    }

private:
    std::unique_ptr<std::byte[]> block_;
    size_t block_size_;
    size_t offset_;
    size_t allocation_count_ = 0;
    size_t total_allocated_ = 0;
    size_t peak_offset_ = 0;
};

/// @brief Stack-based arena with scope-bound lifetimes
class ScopedArena : public ArenaAllocator {
public:
    explicit ScopedArena(size_t block_size) : ArenaAllocator(block_size) {}
    ~ScopedArena() { reset(); }

    ScopedArena(const ScopedArena&) = delete;
    ScopedArena& operator=(const ScopedArena&) = delete;
};

/// @brief Monotonic buffer resource (std::pmr compatible if using C++17 polymorphic allocators)
/// Simple wrapper for use with code that expects std::pmr interfaces
class MonotonicBufferResource {
public:
    explicit MonotonicBufferResource(size_t initial_size = 4096) {
        reserve(initial_size);
    }

    [[nodiscard]] void* allocate(size_t bytes, size_t alignment = alignof(std::max_align_t)) {
        size_t aligned_offset = (offset_ + alignment - 1) & ~(alignment - 1);
        if (aligned_offset + bytes > buffer_.size()) {
            buffer_.resize(std::max(buffer_.size() * 2, aligned_offset + bytes));
        }
        void* ptr = buffer_.data() + aligned_offset;
        offset_ = aligned_offset + bytes;
        ++count_;
        return ptr;
    }

    void deallocate(void*, size_t, size_t = 0) noexcept {}

    void release() noexcept {
        buffer_.clear();
        offset_ = 0;
        count_ = 0;
    }

    void reserve(size_t bytes) { buffer_.reserve(bytes); }
    [[nodiscard]] size_t used() const noexcept { return offset_; }
    [[nodiscard]] size_t capacity() const noexcept { return buffer_.capacity(); }
    [[nodiscard]] size_t allocationCount() const noexcept { return count_; }

private:
    std::vector<std::byte> buffer_;
    size_t offset_ = 0;
    size_t count_ = 0;
};

} // namespace nexus::utility::allocator
