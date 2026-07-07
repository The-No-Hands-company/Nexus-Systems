#pragma once

#include <string>
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <optional>
#include <ostream>
#include <iostream>

namespace nexus::utility::concurrency {

/// @brief Registry for tracking active threads and their names
class ThreadTracker {
public:
    [[nodiscard]] static ThreadTracker& instance();

    /// @brief Register the current thread with a name
    void registerThread(const std::string& name);

    /// @brief Unregister the current thread
    void unregisterThread();

    /// @brief Get the name of a specific thread
    [[nodiscard]] std::optional<std::string> getThreadName(std::thread::id id) const;

    /// @brief Get name of current thread
    [[nodiscard]] std::string getCurrentThreadName() const;

    /// @brief Get count of currently registered threads
    [[nodiscard]] size_t getActiveThreadCount() const;

    /// @brief Print a thread report to output stream
    void printReport(std::ostream& os = std::cout) const;

private:
    ThreadTracker() = default;
    ~ThreadTracker() = default;

    ThreadTracker(const ThreadTracker&) = delete;
    ThreadTracker& operator=(const ThreadTracker&) = delete;

    mutable std::shared_mutex mutex_;
    std::unordered_map<std::thread::id, std::string> threads_;
};

/// @brief RAII helper to register a thread for its duration
class ScopedThreadRegistration {
public:
    explicit ScopedThreadRegistration(const std::string& name) {
        ThreadTracker::instance().registerThread(name);
    }

    ~ScopedThreadRegistration() {
        ThreadTracker::instance().unregisterThread();
    }

    ScopedThreadRegistration(const ScopedThreadRegistration&) = delete;
    ScopedThreadRegistration& operator=(const ScopedThreadRegistration&) = delete;
};

/// @brief RAII thread that auto-registers with ThreadTracker
class TrackedThread {
public:
    template <typename Callable, typename... Args>
    explicit TrackedThread(const std::string& name, Callable&& func, Args&&... args)
        : name_(name)
        , thread_([this, f = std::forward<Callable>(func), ... args = std::forward<Args>(args)]() mutable {
            ScopedThreadRegistration reg(name_);
            f(std::forward<Args>(args)...);
        }) {}

    ~TrackedThread() {
        if (thread_.joinable()) {
            thread_.join();
        }
    }

    TrackedThread(const TrackedThread&) = delete;
    TrackedThread& operator=(const TrackedThread&) = delete;

    void join() {
        if (thread_.joinable()) {
            thread_.join();
        }
    }

    [[nodiscard]] std::thread::id get_id() const noexcept {
        return thread_.get_id();
    }

private:
    std::string name_;
    std::thread thread_;
};

} // namespace nexus::utility::concurrency
