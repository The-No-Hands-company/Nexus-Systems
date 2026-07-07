#pragma once

#include <unordered_set>
#include <mutex>
#include <stdexcept>

namespace nexus::utility::memory {

/**
 * @brief Detects double-free attempts.
 */
class DoubleFreeDetector {
public:
    /**
     * @brief Records a pointer being freed.
     */
    static void recordFree(void* ptr) {
        if (!ptr) return;
        
        std::lock_guard lock(mutex_);
        
        // Check if already freed
        if (freed_ptrs_.find(ptr) != freed_ptrs_.end()) {
            throw std::runtime_error("Double-free detected!");
        }
        
        freed_ptrs_.insert(ptr);
    }

    /**
     * @brief Records a pointer being allocated (reused).
     */
    static void recordAlloc(void* ptr) {
        if (!ptr) return;
        
        std::lock_guard lock(mutex_);
        freed_ptrs_.erase(ptr);
    }

    /**
     * @brief Checks if a pointer was already freed.
     */
    static bool wasFreed(void* ptr) {
        if (!ptr) return false;
        
        std::lock_guard lock(mutex_);
        return freed_ptrs_.find(ptr) != freed_ptrs_.end();
    }

    /**
     * @brief Gets count of tracked freed pointers.
     */
    static size_t getFreedCount() {
        std::lock_guard lock(mutex_);
        return freed_ptrs_.size();
    }

    /**
     * @brief Resets tracking.
     */
    static void reset() {
        std::lock_guard lock(mutex_);
        freed_ptrs_.clear();
    }

    /**
     * @brief RAII wrapper for tracking allocations/frees.
     */
    template<typename T>
    class TrackedPtr {
    public:
        explicit TrackedPtr(T* ptr = nullptr) : ptr_(ptr) {
            if (ptr_) {
                recordAlloc(ptr_);
            }
        }

        ~TrackedPtr() {
            if (ptr_) {
                recordFree(ptr_);
                delete ptr_;
                ptr_ = nullptr;
            }
        }

        // Non-copyable
        TrackedPtr(const TrackedPtr&) = delete;
        TrackedPtr& operator=(const TrackedPtr&) = delete;

        // Movable
        TrackedPtr(TrackedPtr&& other) noexcept : ptr_(other.ptr_) {
            other.ptr_ = nullptr;
        }

        TrackedPtr& operator=(TrackedPtr&& other) noexcept {
            if (this != &other) {
                if (ptr_) {
                    recordFree(ptr_);
                    delete ptr_;
                }
                ptr_ = other.ptr_;
                other.ptr_ = nullptr;
            }
            return *this;
        }

        T* get() const { return ptr_; }
        T* operator->() const { return ptr_; }
        T& operator*() const { return *ptr_; }

    private:
        T* ptr_;
    };

private:
    static inline std::mutex mutex_;
    static inline std::unordered_set<void*> freed_ptrs_;
};

} // namespace nexus::utility::memory
