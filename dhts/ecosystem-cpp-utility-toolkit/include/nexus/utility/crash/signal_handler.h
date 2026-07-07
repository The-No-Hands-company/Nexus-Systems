#pragma once

#include <csignal>
#include <functional>
#include <atomic>
#include <cstring>
#include <unistd.h>
#include <string>

namespace nexus::utility::crash {

/// @brief POSIX signal handler for crash signals
/// Handles SIGSEGV, SIGABRT, SIGFPE, SIGILL, SIGBUS, SIGTRAP
class SignalHandler {
public:
    using SignalCallback = void (*)(int signal, siginfo_t* info, void* context);
    using CrashCallback = std::function<void(int sig, const std::string& info)>;

    [[nodiscard]] static SignalHandler& instance() {
        static SignalHandler handler;
        return handler;
    }

    /// @brief Installs signal handlers for common crash signals
    void install(SignalCallback callback = nullptr) {
        if (callback) {
            user_callback_ = callback;
        }

        struct sigaction sa {};
        std::memset(&sa, 0, sizeof(sa));
        sa.sa_sigaction = signalHandler;
        sa.sa_flags = SA_SIGINFO | SA_RESTART;
        sigemptyset(&sa.sa_mask);

        sigaction(SIGSEGV, &sa, &old_sigsegv_);
        sigaction(SIGABRT, &sa, &old_sigabrt_);
        sigaction(SIGFPE,  &sa, &old_sigfpe_);
        sigaction(SIGILL,  &sa, &old_sigill_);
        sigaction(SIGBUS,  &sa, &old_sigbus_);
        sigaction(SIGTRAP, &sa, &old_sigtrap_);

        installed_.store(true, std::memory_order_release);
    }

    /// @brief Uninstalls signal handlers, restoring previous handlers
    void uninstall() {
        if (!installed_.load(std::memory_order_acquire)) {
            return;
        }

        sigaction(SIGSEGV, &old_sigsegv_, nullptr);
        sigaction(SIGABRT, &old_sigabrt_, nullptr);
        sigaction(SIGFPE,  &old_sigfpe_,  nullptr);
        sigaction(SIGILL,  &old_sigill_,  nullptr);
        sigaction(SIGBUS,  &old_sigbus_,  nullptr);
        sigaction(SIGTRAP, &old_sigtrap_, nullptr);

        installed_.store(false, std::memory_order_release);
    }

    /// @brief Set a crash callback for custom handling
    void setCrashCallback(CrashCallback callback) {
        crash_callback_ = std::move(callback);

        if (!installed_.load(std::memory_order_acquire)) {
            install(nullptr);
        }
    }

    /// @brief Get signal name as string (async-signal-safe)
    [[nodiscard]] static const char* getSignalName(int signal) noexcept {
        switch (signal) {
            case SIGSEGV: return "SIGSEGV (Segmentation fault)";
            case SIGABRT: return "SIGABRT (Abort)";
            case SIGFPE:  return "SIGFPE (Floating point exception)";
            case SIGILL:  return "SIGILL (Illegal instruction)";
            case SIGBUS:  return "SIGBUS (Bus error)";
            case SIGTRAP: return "SIGTRAP (Trace/breakpoint trap)";
            default:      return "Unknown signal";
        }
    }

    // ── Alias methods for compatibility ─────────────────────────────────────
    void installHandlers() { install(nullptr); }
    void restoreDefaultHandlers() { uninstall(); }
    [[nodiscard]] bool isInstalled() const noexcept {
        return installed_.load(std::memory_order_acquire);
    }

    SignalHandler(const SignalHandler&) = delete;
    SignalHandler& operator=(const SignalHandler&) = delete;

private:
    SignalHandler() = default;
    ~SignalHandler() { uninstall(); }

    static void signalHandler(int signal, siginfo_t* info, void* context) {
        auto& inst = instance();

        const char* msg = "\n=== CRASH DETECTED ===\nSignal: ";
        write(STDERR_FILENO, msg, strlen(msg));
        write(STDERR_FILENO, getSignalName(signal), strlen(getSignalName(signal)));
        write(STDERR_FILENO, "\n", 1);

        if (inst.user_callback_) {
            inst.user_callback_(signal, info, context);
        }

        if (inst.crash_callback_) {
            inst.crash_callback_(signal, getSignalName(signal));
        }

        struct sigaction* old_handler = nullptr;
        switch (signal) {
            case SIGSEGV: old_handler = &inst.old_sigsegv_; break;
            case SIGABRT: old_handler = &inst.old_sigabrt_; break;
            case SIGFPE:  old_handler = &inst.old_sigfpe_;  break;
            case SIGILL:  old_handler = &inst.old_sigill_;  break;
            case SIGBUS:  old_handler = &inst.old_sigbus_;  break;
            case SIGTRAP: old_handler = &inst.old_sigtrap_; break;
        }

        if (old_handler && old_handler->sa_sigaction) {
            old_handler->sa_sigaction(signal, info, context);
        } else {
            _exit(128 + signal);
        }
    }

    std::atomic<bool> installed_{false};
    SignalCallback user_callback_ = nullptr;
    CrashCallback crash_callback_;

    struct sigaction old_sigsegv_ {};
    struct sigaction old_sigabrt_ {};
    struct sigaction old_sigfpe_ {};
    struct sigaction old_sigill_ {};
    struct sigaction old_sigbus_ {};
    struct sigaction old_sigtrap_ {};
};

} // namespace nexus::utility::crash
