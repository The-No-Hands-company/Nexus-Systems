#pragma once

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <cstddef>

namespace nexus::utility::allocator {

/**
 * @brief Validate reference-counted objects and detect leaks.
 *
 * Tracks AddRef/Release calls per object pointer and maintains a live reference
 * count. Objects whose reference count is still positive at leak-detection time
 * are reported as leaked; a negative count would indicate an over-release bug.
 *
 * Thread safety: all public methods are protected by a mutex.
 *
 * @category Allocator
 * @version 1.0.0
 */
class ReferenceCountingValidator {
public:
    /// @brief Get singleton instance
    static ReferenceCountingValidator& instance() {
        static ReferenceCountingValidator inst;
        return inst;
    }

    /// @brief Initialize the utility
    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = config;
        enabled_ = true;
        counts_.clear();
        over_releases_ = 0;
    }

    /// @brief Shutdown the utility and cleanup resources
    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
        counts_.clear();
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

    /// @brief Record an AddRef on the given object pointer.
    void recordAddRef(void* obj) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!enabled_) return;
        ++counts_[obj];
    }

    /// @brief Record a Release on the given object pointer.
    void recordRelease(void* obj) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!enabled_) return;
        auto& c = counts_[obj];
        if (c <= 0) {
            ++over_releases_;
        }
        --c;
    }

    /// @brief Current reference count for an object (0 if unknown).
    long getRefCount(void* obj) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = counts_.find(obj);
        if (it == counts_.end()) return 0;
        return it->second;
    }

    /// @brief Detect leaks: objects whose reference count is still positive.
    /// @return Vector of pointers with a live (> 0) reference count.
    std::vector<void*> detectLeaks() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<void*> leaks;
        for (const auto& [obj, count] : counts_) {
            if (count > 0) leaks.push_back(obj);
        }
        return leaks;
    }

    /// @brief Number of leaked objects (ref count > 0).
    std::size_t getLeakCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::size_t leaks = 0;
        for (const auto& [obj, count] : counts_) {
            if (count > 0) ++leaks;
        }
        return leaks;
    }

    /// @brief Number of over-release (release below zero) events observed.
    std::size_t getOverReleaseCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return over_releases_;
    }

    /// @brief Number of distinct objects tracked.
    std::size_t getTrackedCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return counts_.size();
    }

    /// @brief Get utility statistics/status
    std::string getStatus() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::size_t leaks = 0;
        for (const auto& [obj, count] : counts_) {
            if (count > 0) ++leaks;
        }
        return "ReferenceCountingValidator[tracked=" + std::to_string(counts_.size()) +
               ", leaks=" + std::to_string(leaks) +
               ", over_releases=" + std::to_string(over_releases_) + "]";
    }

    /// @brief Reset utility state
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        counts_.clear();
        over_releases_ = 0;
    }

private:
    ReferenceCountingValidator() = default;
    ~ReferenceCountingValidator() = default;

    ReferenceCountingValidator(const ReferenceCountingValidator&) = delete;
    ReferenceCountingValidator& operator=(const ReferenceCountingValidator&) = delete;
    ReferenceCountingValidator(ReferenceCountingValidator&&) = delete;
    ReferenceCountingValidator& operator=(ReferenceCountingValidator&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::map<void*, long> counts_;
    std::size_t over_releases_ = 0;
};

} // namespace nexus::utility::allocator
