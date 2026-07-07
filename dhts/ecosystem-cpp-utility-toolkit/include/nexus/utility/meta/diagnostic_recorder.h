#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <chrono>
#include <source_location>
#include <shared_mutex>
#include <algorithm>
#include <cstddef>
#include <utility>

namespace nexus::utility::meta {

/// @brief Structured diagnostic event record
struct DiagnosticEvent {
    std::chrono::system_clock::time_point timestamp;
    std::string message;
    std::source_location location;
    std::string severity;  // "info", "warning", "error"
    int64_t sequence = 0;
};

/// @brief Base class for diagnostic recorders.
/// Provides thread-safe singleton pattern with event recording, history,
/// enable/disable, and statistics. Derived classes add domain-specific
/// recording methods that call recordEvent().
///
/// Thread safety: all public methods are protected by a shared_mutex.
///
/// Usage:
///   class MyDiagnostic : public DiagnosticRecorder<MyDiagnostic> {
///   public:
///       void recordSomething(const std::string& data) {
///           recordEvent(data, "info");
///       }
///   };
template <typename Derived>
class DiagnosticRecorder {
public:
    DiagnosticRecorder(const DiagnosticRecorder&) = delete;
    DiagnosticRecorder& operator=(const DiagnosticRecorder&) = delete;

    /// @brief Enable diagnostic recording
    void enable() noexcept {
        std::unique_lock lock(mutex_);
        enabled_ = true;
    }

    /// @brief Disable diagnostic recording (silently discards events)
    void disable() noexcept {
        std::unique_lock lock(mutex_);
        enabled_ = false;
    }

    /// @brief Check if diagnostics are enabled
    [[nodiscard]] bool isEnabled() const noexcept {
        std::shared_lock lock(mutex_);
        return enabled_;
    }

    /// @brief Get all recorded events
    [[nodiscard]] std::vector<DiagnosticEvent> getHistory() const {
        std::shared_lock lock(mutex_);
        return history_;
    }

    /// @brief Get recent events (last N)
    [[nodiscard]] std::vector<DiagnosticEvent> getRecent(size_t n) const {
        std::shared_lock lock(mutex_);
        if (history_.size() <= n) return history_;
        return {history_.end() - static_cast<ptrdiff_t>(n), history_.end()};
    }

    /// @brief Get event count
    [[nodiscard]] size_t getCount() const noexcept {
        std::shared_lock lock(mutex_);
        return history_.size();
    }

    /// @brief Clear all recorded events
    void clear() noexcept {
        std::unique_lock lock(mutex_);
        history_.clear();
        sequence_ = 0;
    }

    /// @brief Set maximum history size (oldest events are dropped)
    void setMaxHistory(size_t max_size) noexcept {
        std::unique_lock lock(mutex_);
        max_history_ = max_size;
        while (history_.size() > max_history_) {
            history_.erase(history_.begin());
        }
    }

    [[nodiscard]] size_t getMaxHistory() const noexcept {
        std::shared_lock lock(mutex_);
        return max_history_;
    }

    /// @brief Get the singleton instance
    [[nodiscard]] static Derived& instance() noexcept {
        static Derived inst;
        return inst;
    }

    // ── CRTP initialization hooks ───────────────────────────────────────────
    void initialize() {
        std::unique_lock lock(mutex_);
        history_.clear();
        sequence_ = 0;
        enabled_ = true;
        static_cast<Derived*>(this)->onInitialize();
    }

    void shutdown() {
        std::unique_lock lock(mutex_);
        enabled_ = false;
        static_cast<Derived*>(this)->onShutdown();
    }

    // Called before the lock is released during initialize
    virtual void onInitialize() {}
    virtual void onShutdown() {}

protected:
    DiagnosticRecorder() = default;
    virtual ~DiagnosticRecorder() = default;

    /// @brief Record a diagnostic event (call from derived class methods)
    void recordEvent(const std::string& message,
                     const std::string& severity = "info",
                     std::source_location loc = std::source_location::current()) {
        std::unique_lock lock(mutex_);
        if (!enabled_) return;

        history_.push_back({
            std::chrono::system_clock::now(),
            message,
            loc,
            severity,
            sequence_++
        });

        while (history_.size() > max_history_) {
            history_.erase(history_.begin());
        }
    }

    /// @brief Record a warning event
    void recordWarning(const std::string& message,
                       std::source_location loc = std::source_location::current()) {
        recordEvent(message, "warning", loc);
    }

    /// @brief Record an error event
    void recordError(const std::string& message,
                     std::source_location loc = std::source_location::current()) {
        recordEvent(message, "error", loc);
    }

    mutable std::shared_mutex mutex_;
    bool enabled_ = false;

private:
    std::vector<DiagnosticEvent> history_;
    int64_t sequence_ = 0;
    size_t max_history_ = 10000;
};

} // namespace nexus::utility::meta
