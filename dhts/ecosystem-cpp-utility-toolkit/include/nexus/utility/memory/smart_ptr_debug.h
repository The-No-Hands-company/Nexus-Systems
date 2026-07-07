#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <string>
#include <sstream>

namespace nexus::utility::memory {

/**
 * @brief Debug tracking for smart pointers.
 */
class SmartPtrDebug {
public:
    struct PtrInfo {
        void* raw_ptr;
        std::string type_name;
        size_t ref_count;
        std::string allocation_site;
    };

    /**
     * @brief Tracks a unique_ptr creation.
     */
    template<typename T>
    static void trackUnique(const std::unique_ptr<T>& ptr, const std::string& site = "") {
        if (ptr) {
            std::lock_guard lock(mutex_);
            PtrInfo info;
            info.raw_ptr = ptr.get();
            info.type_name = typeid(T).name();
            info.ref_count = 1;
            info.allocation_site = site;
            unique_ptrs_[ptr.get()] = info;
        }
    }

    /**
     * @brief Tracks a unique_ptr destruction.
     */
    template<typename T>
    static void untrackUnique(const T* ptr) {
        std::lock_guard lock(mutex_);
        unique_ptrs_.erase(const_cast<T*>(ptr));
    }

    /**
     * @brief Tracks a shared_ptr creation.
     */
    template<typename T>
    static void trackShared(const std::shared_ptr<T>& ptr, const std::string& site = "") {
        if (ptr) {
            std::lock_guard lock(mutex_);
            PtrInfo info;
            info.raw_ptr = ptr.get();
            info.type_name = typeid(T).name();
            info.ref_count = ptr.use_count();
            info.allocation_site = site;
            shared_ptrs_[ptr.get()] = info;
        }
    }

    /**
     * @brief Updates shared_ptr reference count.
     */
    template<typename T>
    static void updateShared(const std::shared_ptr<T>& ptr) {
        if (ptr) {
            std::lock_guard lock(mutex_);
            auto it = shared_ptrs_.find(ptr.get());
            if (it != shared_ptrs_.end()) {
                it->second.ref_count = ptr.use_count();
            }
        }
    }

    /**
     * @brief Checks if a raw pointer is managed.
     */
    static bool isManagedUnique(const void* ptr) {
        std::lock_guard lock(mutex_);
        return unique_ptrs_.find(const_cast<void*>(ptr)) != unique_ptrs_.end();
    }

    /**
     * @brief Checks if a raw pointer is shared.
     */
    static bool isManagedShared(const void* ptr) {
        std::lock_guard lock(mutex_);
        return shared_ptrs_.find(const_cast<void*>(ptr)) != shared_ptrs_.end();
    }

    /**
     * @brief Detects potential dangling pointers.
     */
    static std::vector<void*> detectDanglingPointers() {
        std::lock_guard lock(mutex_);
        std::vector<void*> dangling;
        
        // Check for shared_ptrs with ref_count == 0 (shouldn't happen but indicates issues)
        for (const auto& [ptr, info] : shared_ptrs_) {
            if (info.ref_count == 0) {
                dangling.push_back(ptr);
            }
        }
        
        return dangling;
    }

    /**
     * @brief Generates a report of all tracked pointers.
     */
    static std::string generateReport() {
        std::lock_guard lock(mutex_);
        std::ostringstream report;
        
        report << "=== Smart Pointer Debug Report ===\n";
        report << "Unique Pointers: " << unique_ptrs_.size() << "\n";
        report << "Shared Pointers: " << shared_ptrs_.size() << "\n\n";
        
        report << "=== Unique Pointers ===\n";
        for (const auto& [ptr, info] : unique_ptrs_) {
            report << "Ptr: " << ptr << ", Type: " << info.type_name;
            if (!info.allocation_site.empty()) {
                report << ", Site: " << info.allocation_site;
            }
            report << "\n";
        }
        
        report << "\n=== Shared Pointers ===\n";
        for (const auto& [ptr, info] : shared_ptrs_) {
            report << "Ptr: " << ptr << ", Type: " << info.type_name 
                   << ", RefCount: " << info.ref_count;
            if (!info.allocation_site.empty()) {
                report << ", Site: " << info.allocation_site;
            }
            report << "\n";
        }
        
        return report.str();
    }

    /**
     * @brief Resets all tracking.
     */
    static void reset() {
        std::lock_guard lock(mutex_);
        unique_ptrs_.clear();
        shared_ptrs_.clear();
    }

private:
    static inline std::mutex mutex_;
    static inline std::unordered_map<void*, PtrInfo> unique_ptrs_;
    static inline std::unordered_map<void*, PtrInfo> shared_ptrs_;
};

/**
 * @brief Debug wrapper for unique_ptr.
 */
template<typename T>
class DebugUniquePtr : public std::unique_ptr<T> {
public:
    using Base = std::unique_ptr<T>;
    
    DebugUniquePtr() : Base() {}
    
    explicit DebugUniquePtr(T* ptr, const std::string& site = "") : Base(ptr) {
        SmartPtrDebug::trackUnique(*this, site);
    }
    
    ~DebugUniquePtr() {
        if (this->get()) {
            SmartPtrDebug::untrackUnique(this->get());
        }
    }
};

} // namespace nexus::utility::memory
