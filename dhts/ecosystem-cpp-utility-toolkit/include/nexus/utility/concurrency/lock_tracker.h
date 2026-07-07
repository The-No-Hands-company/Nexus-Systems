#pragma once

#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>
#include <shared_mutex>
#include <set>

namespace nexus::utility::concurrency {

/// @brief Tracks held locks and waiting threads to detect deadlocks
class LockTracker {
public:
    static LockTracker& instance();

    /// @brief Called before attempting to acquire a lock
    void onBeforeLock(const void* mutex_addr);

    /// @brief Called after successfully acquiring a lock
    void onAfterLock(const void* mutex_addr);

    /// @brief Called when unlocking
    void onUnlock(const void* mutex_addr);

    // Accessors for DeadlockDetector
    // We export snapshots or expose internal state safely
    using HeldLocksMap = std::unordered_map<std::thread::id, std::vector<const void*>>;
    using WaitingMap = std::unordered_map<std::thread::id, const void*>;

    HeldLocksMap getSnapshotHeldLocks() const;
    WaitingMap getSnapshotWaiting() const;

private:
    LockTracker() = default;
    ~LockTracker() = default;

    mutable std::shared_mutex mutex_;
    HeldLocksMap held_locks_;
    WaitingMap waiting_for_;
    
    // Reverse map: Lock -> Owner Thread (for O(1) owner lookup during cycle check)
    std::unordered_map<const void*, std::thread::id> lock_owners_;
};

} // namespace nexus::utility::concurrency
