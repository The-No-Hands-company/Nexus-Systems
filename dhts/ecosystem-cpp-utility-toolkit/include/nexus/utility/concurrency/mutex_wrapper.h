#pragma once

#include <mutex>
#include <nexus/utility/concurrency/lock_tracker.h>

namespace nexus::utility::concurrency {

/// @brief Wrapper around std::mutex (or compatible) that integrates with LockTracker
/// @tparam UnderlyingMutex The mutex type (default std::mutex)
template<typename UnderlyingMutex = std::mutex>
class TrackedMutex {
public:
    TrackedMutex() = default;
    ~TrackedMutex() = default;

    TrackedMutex(const TrackedMutex&) = delete;
    TrackedMutex& operator=(const TrackedMutex&) = delete;

    void lock() {
#ifdef NEXUS_ENABLE_LOCK_TRACKING
        LockTracker::instance().onBeforeLock(this);
#endif
        mutex_.lock();
#ifdef NEXUS_ENABLE_LOCK_TRACKING
        LockTracker::instance().onAfterLock(this);
#endif
    }

    bool try_lock() {
        bool result = mutex_.try_lock();
        if (result) {
#ifdef NEXUS_ENABLE_LOCK_TRACKING
            LockTracker::instance().onAfterLock(this);
#endif
        }
        return result;
    }

    void unlock() {
#ifdef NEXUS_ENABLE_LOCK_TRACKING
        LockTracker::instance().onUnlock(this);
#endif
        mutex_.unlock();
    }

    /// @brief Access underlying mutex if strictly needed
    UnderlyingMutex& native_handle() { return mutex_; }

private:
    UnderlyingMutex mutex_;
};

} // namespace nexus::utility::concurrency
