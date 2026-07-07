#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <memory>

namespace nexus::utility::containers {

/**
 * @brief Thread-safe queue with blocking and non-blocking operations.
 */
template<typename T>
class ThreadSafeQueue {
public:
    ThreadSafeQueue() = default;

    /**
     * @brief Pushes an item to the queue.
     */
    void push(T value) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(std::move(value));
        cond_.notify_one();
    }

    /**
     * @brief Tries to pop an item without blocking.
     * @return std::optional with value if available, std::nullopt otherwise.
     */
    [[nodiscard]] std::optional<T> tryPop() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return std::nullopt;
        }
        T value = std::move(queue_.front());
        queue_.pop();
        return value;
    }

    /**
     * @brief Waits and pops an item (blocking).
     */
    T waitAndPop() {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_.wait(lock, [this] { return !queue_.empty(); });
        T value = std::move(queue_.front());
        queue_.pop();
        return value;
    }

    /**
     * @brief Checks if queue is empty.
     */
    [[nodiscard]] bool empty() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    /**
     * @brief Returns the size of the queue.
     */
    [[nodiscard]] size_t size() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

private:
    mutable std::mutex mutex_;
    std::queue<T> queue_;
    std::condition_variable cond_;
};

} // namespace nexus::utility::containers
