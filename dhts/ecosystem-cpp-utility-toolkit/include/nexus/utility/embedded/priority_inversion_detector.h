#pragma once

#include <string>
#include <map>
#include <mutex>

namespace nexus::utility::embedded {

class PriorityInversionDetector {
public:
    static PriorityInversionDetector& instance() {
        static PriorityInversionDetector inst;
        return inst;
    }

    void initialize() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = true; }
    void shutdown() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = false; }
    bool isEnabled() const { return enabled_; }

    void recordLockAcquire(int priority, const std::string& lock) {
        std::lock_guard<std::mutex> lk(mutex_);
        if (!enabled_) return;
        lock_owners_[lock] = priority;
    }

    void recordLockRelease(const std::string& lock) {
        std::lock_guard<std::mutex> lk(mutex_);
        if (!enabled_) return;
        lock_owners_.erase(lock);
    }

    bool checkInversion() const {
        std::lock_guard<std::mutex> lk(mutex_);
        for (const auto& [lock_name, owner_prio] : lock_owners_) {
            auto it = waiting_tasks_.find(lock_name);
            if (it != waiting_tasks_.end() && it->second > owner_prio) {
                return true;
            }
        }
        return false;
    }

    void recordLockWait(int priority, const std::string& lock) {
        std::lock_guard<std::mutex> lk(mutex_);
        if (!enabled_) return;
        waiting_tasks_[lock] = priority;
    }

private:
    PriorityInversionDetector() = default;
    ~PriorityInversionDetector() = default;
    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::map<std::string, int> lock_owners_;    // lock_name -> owner priority
    std::map<std::string, int> waiting_tasks_;   // lock_name -> waiting task priority
};

} // namespace nexus::utility::embedded
