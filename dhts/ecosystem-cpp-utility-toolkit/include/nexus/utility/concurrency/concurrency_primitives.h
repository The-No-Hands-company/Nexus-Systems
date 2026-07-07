#pragma once

#include <mutex>
#include <condition_variable>
#include <atomic>
#include <cstddef>
#include <shared_mutex>

namespace nexus::utility::concurrency {

/// @brief Counting semaphore for thread synchronization
class Semaphore {
public:
    explicit Semaphore(ptrdiff_t initial_count = 0) noexcept
        : count_(initial_count) {}

    /// Acquire the semaphore (decrement). Blocks if count is zero.
    void acquire() {
        std::unique_lock lock(mutex_);
        cv_.wait(lock, [this] { return count_ > 0; });
        --count_;
    }

    /// Try to acquire without blocking. Returns true if successful.
    [[nodiscard]] bool tryAcquire() noexcept {
        std::lock_guard lock(mutex_);
        if (count_ > 0) {
            --count_;
            return true;
        }
        return false;
    }

    /// Release the semaphore (increment). Wakes one waiting thread.
    void release(ptrdiff_t n = 1) noexcept {
        {
            std::lock_guard lock(mutex_);
            count_ += n;
        }
        if (n > 1) {
            cv_.notify_all();
        } else {
            cv_.notify_one();
        }
    }

    [[nodiscard]] ptrdiff_t available() const noexcept {
        std::lock_guard lock(mutex_);
        return count_;
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    ptrdiff_t count_;
};

/// @brief Barrier for synchronizing a group of threads at a rendezvous point
class Barrier {
public:
    explicit Barrier(size_t thread_count) noexcept
        : threshold_(thread_count), count_(thread_count), generation_(0) {}

    /// Wait at the barrier. Blocks until all threads arrive.
    /// Returns true for exactly one thread (the "leader").
    [[nodiscard]] bool wait() {
        std::unique_lock lock(mutex_);
        size_t gen = generation_;
        if (--count_ == 0) {
            ++generation_;
            count_ = threshold_;
            lock.unlock();
            cv_.notify_all();
            return true;
        }
        cv_.wait(lock, [this, gen] { return generation_ != gen; });
        return false;
    }

    /// Reset the barrier to the initial count
    void reset() noexcept {
        std::lock_guard lock(mutex_);
        count_ = threshold_;
        ++generation_;
        cv_.notify_all();
    }

    [[nodiscard]] size_t waiting() const noexcept {
        std::lock_guard lock(mutex_);
        return threshold_ - count_;
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    size_t threshold_;
    size_t count_;
    size_t generation_;
};

/// @brief Single-use latch that opens once a count is reached
class Latch {
public:
    explicit Latch(ptrdiff_t count) noexcept
        : count_(count) {}

    /// Decrement the latch count. The last thread wakes all waiters.
    void countDown(ptrdiff_t n = 1) noexcept {
        std::lock_guard lock(mutex_);
        count_ -= n;
        if (count_ <= 0) {
            count_ = 0;
            cv_.notify_all();
        }
    }

    /// Wait until the latch reaches zero
    void wait() const {
        std::unique_lock lock(mutex_);
        cv_.wait(lock, [this] { return count_ <= 0; });
    }

    /// Try waiting with a check. Returns true if latch is open.
    [[nodiscard]] bool tryWait() const noexcept {
        std::lock_guard lock(mutex_);
        return count_ <= 0;
    }

    [[nodiscard]] ptrdiff_t remaining() const noexcept {
        std::lock_guard lock(mutex_);
        return count_;
    }

private:
    mutable std::mutex mutex_;
    mutable std::condition_variable cv_;
    ptrdiff_t count_;
};

/// @brief Read-write lock with preference policy
class ReadWriteLock {
public:
    enum class Policy { PreferReaders, PreferWriters, Fair };

    explicit ReadWriteLock(Policy policy = Policy::Fair) noexcept
        : policy_(policy), readers_(0), writers_waiting_(0), writing_(false) {}

    void lockRead() {
        std::unique_lock lock(mutex_);
        if (policy_ == Policy::PreferWriters && writers_waiting_ > 0) {
            cv_read_.wait(lock, [this] { return !writing_ && writers_waiting_ == 0; });
        } else {
            cv_read_.wait(lock, [this] { return !writing_; });
        }
        ++readers_;
    }

    void unlockRead() noexcept {
        std::unique_lock lock(mutex_);
        if (--readers_ == 0 && writers_waiting_ > 0) {
            lock.unlock();
            cv_write_.notify_one();
        } else if (readers_ == 0) {
            lock.unlock();
            cv_write_.notify_one();
        }
    }

    void lockWrite() {
        std::unique_lock lock(mutex_);
        ++writers_waiting_;
        cv_write_.wait(lock, [this] { return readers_ == 0 && !writing_; });
        --writers_waiting_;
        writing_ = true;
    }

    void unlockWrite() noexcept {
        std::unique_lock lock(mutex_);
        writing_ = false;
        if (writers_waiting_ > 0 && policy_ != Policy::PreferReaders) {
            lock.unlock();
            cv_write_.notify_one();
        } else {
            lock.unlock();
            cv_read_.notify_all();
        }
    }

    /// RAII read lock
    class ReadGuard {
    public:
        explicit ReadGuard(ReadWriteLock& rwlock) : rwlock_(rwlock) { rwlock_.lockRead(); }
        ~ReadGuard() { rwlock_.unlockRead(); }
        ReadGuard(const ReadGuard&) = delete;
        ReadGuard& operator=(const ReadGuard&) = delete;
    private:
        ReadWriteLock& rwlock_;
    };

    /// RAII write lock
    class WriteGuard {
    public:
        explicit WriteGuard(ReadWriteLock& rwlock) : rwlock_(rwlock) { rwlock_.lockWrite(); }
        ~WriteGuard() { rwlock_.unlockWrite(); }
        WriteGuard(const WriteGuard&) = delete;
        WriteGuard& operator=(const WriteGuard&) = delete;
    private:
        ReadWriteLock& rwlock_;
    };

private:
    Policy policy_;
    mutable std::mutex mutex_;
    std::condition_variable cv_read_;
    std::condition_variable cv_write_;
    size_t readers_;
    size_t writers_waiting_;
    bool writing_;
};

} // namespace nexus::utility::concurrency
