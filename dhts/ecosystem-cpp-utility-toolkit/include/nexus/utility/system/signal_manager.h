#pragma once

#include <csignal>
#include <functional>
#include <unordered_map>
#include <atomic>
#include <mutex>

namespace nexus::utility::system {

/// @brief Application-level signal management for graceful shutdown.
///
/// Installs signal handlers for SIGINT, SIGTERM, and optionally SIGHUP.
/// Uses an atomic flag for shutdown notification to avoid non-async-signal-safe
/// operations in signal handler context. User-registered callbacks are deferred
/// to polling via checkPendingSignals().
///
/// Thread safety: registerHandler uses mutex. Signal dispatcher is lock-free.
class SignalManager {
public:
    using SignalHandler = std::function<void(int)>;

    /// @brief Register a callback to be invoked when checkPendingSignals() is called.
    /// The callback runs in a normal (non-signal) context and may allocate/io.
    static void registerHandler(int signal, SignalHandler handler) {
        std::lock_guard lock(handlers_mutex_);
        handlers_[signal] = std::move(handler);

        struct sigaction sa {};
        sa.sa_handler = signalDispatcher;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_RESTART;
        sigaction(signal, &sa, nullptr);
    }

    /// @brief Set up default graceful shutdown handlers (SIGINT + SIGTERM)
    static void installDefaultHandlers() {
        registerHandler(SIGINT, [](int) { requestShutdown(); });
        registerHandler(SIGTERM, [](int) { requestShutdown(); });

        struct sigaction sa {};
        sa.sa_handler = signalDispatcher;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_RESTART;
        sigaction(SIGINT, &sa, nullptr);
        sigaction(SIGTERM, &sa, nullptr);
    }

    /// @brief Block a signal
    static void block(int signal) {
        sigset_t set;
        sigemptyset(&set);
        sigaddset(&set, signal);
        sigprocmask(SIG_BLOCK, &set, nullptr);
    }

    /// @brief Unblock a signal
    static void unblock(int signal) {
        sigset_t set;
        sigemptyset(&set);
        sigaddset(&set, signal);
        sigprocmask(SIG_UNBLOCK, &set, nullptr);
    }

    /// @brief Poll for pending signals and invoke registered callbacks.
    /// Call this periodically from your main loop. Returns true if shutdown was requested.
    [[nodiscard]] static bool checkPendingSignals() {
        bool sigint = pending_.exchange(false, std::memory_order_acquire);
        bool sigterm = pending_term_.exchange(false, std::memory_order_acquire);

        if (sigint) invokeHandler(SIGINT);
        if (sigterm) invokeHandler(SIGTERM);

        return shutdownRequested();
    }

    /// @brief Check if SIGINT/SIGTERM has been received
    [[nodiscard]] static bool shutdownRequested() noexcept {
        return shutdown_requested_.load(std::memory_order_acquire);
    }

    /// @brief Manually request shutdown
    static void requestShutdown() noexcept {
        shutdown_requested_.store(true, std::memory_order_release);
    }

    /// @brief Wait for shutdown signal (blocks until SIGINT/SIGTERM)
    static void waitForShutdown() {
        sigset_t set;
        sigemptyset(&set);
        sigaddset(&set, SIGINT);
        sigaddset(&set, SIGTERM);

        int sig = 0;
        sigwait(&set, &sig);

        requestShutdown();
        invokeHandler(sig);
    }

    /// @brief Unregister a handler
    static void unregisterHandler(int sig) {
        std::lock_guard lock(handlers_mutex_);
        handlers_.erase(sig);
        std::signal(sig, SIG_DFL);
    }

private:
    static void signalDispatcher(int signal) {
        // Async-signal-safe operations only!
        // We just set flags - user callbacks are invoked via checkPendingSignals()
        switch (signal) {
            case SIGINT:
                pending_.store(true, std::memory_order_release);
                break;
            case SIGTERM:
                pending_term_.store(true, std::memory_order_release);
                break;
            default:
                break;
        }
    }

    static void invokeHandler(int signal) {
        SignalHandler handler;
        {
            std::lock_guard lock(handlers_mutex_);
            auto it = handlers_.find(signal);
            if (it != handlers_.end()) {
                handler = it->second;
            }
        }
        if (handler) {
            handler(signal);
        }
    }

    static inline std::unordered_map<int, SignalHandler> handlers_;
    static inline std::mutex handlers_mutex_;
    static inline std::atomic<bool> pending_{false};
    static inline std::atomic<bool> pending_term_{false};
    static inline std::atomic<bool> shutdown_requested_{false};
};

/// @brief RAII guard that installs signal handlers on construction and
/// restores defaults on destruction. Convenient for main() scope.
class ScopedSignalHandler {
public:
    ScopedSignalHandler() {
        SignalManager::installDefaultHandlers();
    }
    ~ScopedSignalHandler() {
        SignalManager::unregisterHandler(SIGINT);
        SignalManager::unregisterHandler(SIGTERM);
    }
    ScopedSignalHandler(const ScopedSignalHandler&) = delete;
    ScopedSignalHandler& operator=(const ScopedSignalHandler&) = delete;
};

} // namespace nexus::utility::system
