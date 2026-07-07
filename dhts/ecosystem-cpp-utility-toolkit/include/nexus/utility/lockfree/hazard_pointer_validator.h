#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <atomic>
#include <mutex>

namespace nexus::utility::lockfree {

/**
 * @brief Track hazard pointer usage and validate safe reclamation.
 */
class HazardPointerValidator {
public:
    static HazardPointerValidator& instance() {
        static HazardPointerValidator inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    /// A thread protects a pointer via a hazard pointer.
    void protect(std::size_t threadId, void* ptr) {
        std::lock_guard<std::mutex> lk(mutex_);
        hazards_[threadId] = ptr;
        ++protects_;
    }
    void clear(std::size_t threadId) {
        std::lock_guard<std::mutex> lk(mutex_);
        hazards_.erase(threadId);
    }

    /// Is a pointer currently protected by any thread?
    bool isProtected(void* ptr) const {
        std::lock_guard<std::mutex> lk(mutex_);
        for (const auto& [tid, p] : hazards_) if (p == ptr) return true;
        return false;
    }

    /// Attempt to retire/free a pointer; unsafe if still hazarded.
    bool retire(void* ptr) {
        std::lock_guard<std::mutex> lk(mutex_);
        for (const auto& [tid, p] : hazards_) {
            if (p == ptr) { ++unsafeReclaims_; return false; }
        }
        ++safeReclaims_;
        return true;
    }

    std::size_t activeHazards() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return hazards_.size();
    }
    std::size_t safeReclaims() const { return safeReclaims_.load(); }
    std::size_t unsafeReclaims() const { return unsafeReclaims_.load(); }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        hazards_.clear();
        protects_ = 0;
        safeReclaims_ = 0;
        unsafeReclaims_ = 0;
    }

private:
    HazardPointerValidator() = default;
    ~HazardPointerValidator() = default;
    HazardPointerValidator(const HazardPointerValidator&) = delete;
    HazardPointerValidator& operator=(const HazardPointerValidator&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::unordered_map<std::size_t, void*> hazards_;
    std::atomic<std::size_t> protects_{0};
    std::atomic<std::size_t> safeReclaims_{0};
    std::atomic<std::size_t> unsafeReclaims_{0};
};

} // namespace nexus::utility::lockfree
