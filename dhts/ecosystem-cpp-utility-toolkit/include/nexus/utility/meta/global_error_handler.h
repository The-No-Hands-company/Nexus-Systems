#pragma once

#include <string>
#include <functional>
#include <mutex>
#include <cstddef>

namespace nexus::utility::meta {

/**
 * @brief Global error handler dispatch point.
 *
 * A single handler (function<void(string,int)>) can be installed to receive
 * error messages and integer codes. handleError() forwards to the handler,
 * counts errors, and remembers the most recent error message and code.
 *
 * Thread safety: all public methods are protected by a mutex.
 *
 * @category Meta
 * @version 1.0.0
 */
class GlobalErrorHandler {
public:
    using Handler = std::function<void(const std::string&, int)>;

    /// @brief Get singleton instance
    static GlobalErrorHandler& instance() {
        static GlobalErrorHandler inst;
        return inst;
    }

    /// @brief Initialize the utility
    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = config;
        enabled_ = true;
        error_count_ = 0;
        last_error_.clear();
        last_code_ = 0;
    }

    /// @brief Shutdown the utility and cleanup resources
    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
        handler_ = nullptr;
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

    /// @brief Install the global error handler.
    void setHandler(Handler handler) {
        std::lock_guard<std::mutex> lock(mutex_);
        handler_ = std::move(handler);
    }

    /// @brief Report an error. Records it and forwards to the installed handler
    /// (if any and if enabled).
    void handleError(const std::string& message, int code) {
        Handler handler;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            ++error_count_;
            last_error_ = message;
            last_code_ = code;
            if (!enabled_) return;
            handler = handler_;
        }
        if (handler) handler(message, code);
    }

    /// @brief Number of errors reported.
    std::size_t getErrorCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return error_count_;
    }

    /// @brief Most recent error message.
    std::string getLastError() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return last_error_;
    }

    /// @brief Most recent error code.
    int getLastCode() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return last_code_;
    }

    /// @brief Whether a handler is currently installed.
    bool hasHandler() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return static_cast<bool>(handler_);
    }

    /// @brief Get utility statistics/status
    std::string getStatus() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return "GlobalErrorHandler[errors=" + std::to_string(error_count_) +
               ", last_code=" + std::to_string(last_code_) + "]";
    }

    /// @brief Reset utility state
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        error_count_ = 0;
        last_error_.clear();
        last_code_ = 0;
    }

private:
    GlobalErrorHandler() = default;
    ~GlobalErrorHandler() = default;

    GlobalErrorHandler(const GlobalErrorHandler&) = delete;
    GlobalErrorHandler& operator=(const GlobalErrorHandler&) = delete;
    GlobalErrorHandler(GlobalErrorHandler&&) = delete;
    GlobalErrorHandler& operator=(GlobalErrorHandler&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    Handler handler_;
    std::size_t error_count_ = 0;
    std::string last_error_;
    int last_code_ = 0;
};

} // namespace nexus::utility::meta
