#pragma once

#include <string>
#include <vector>
#include <map>
#include <unordered_set>
#include <functional>
#include <mutex>
#include <cstddef>

namespace nexus::utility::meta {

/**
 * @brief Ordered registry of initialization callbacks.
 *
 * Initializers are registered by name with a callback. runAll() executes them in
 * registration order and records that order. Names are de-duplicated so
 * re-registering a name replaces its callback but preserves its position.
 *
 * Thread safety: all public methods are protected by a mutex.
 *
 * @category Meta
 * @version 1.0.0
 */
class DiagnosticsInitializer {
public:
    using InitFn = std::function<void()>;

    /// @brief Get singleton instance
    static DiagnosticsInitializer& instance() {
        static DiagnosticsInitializer inst;
        return inst;
    }

    /// @brief Initialize the utility
    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = config;
        enabled_ = true;
    }

    /// @brief Shutdown the utility and cleanup resources
    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
        order_.clear();
        callbacks_.clear();
        run_order_.clear();
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

    /// @brief Register an initializer callback under a name. Re-registering an
    /// existing name replaces its callback but keeps its original position.
    void registerInitializer(const std::string& name, InitFn fn) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (callbacks_.find(name) == callbacks_.end()) {
            order_.push_back(name);
        }
        callbacks_[name] = std::move(fn);
    }

    /// @brief Run all registered initializers in registration order.
    /// @return Number of initializers executed.
    std::size_t runAll() {
        std::vector<std::string> to_run;
        std::vector<InitFn> fns;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!enabled_) return 0;
            to_run = order_;
            fns.reserve(order_.size());
            for (const auto& name : order_) {
                fns.push_back(callbacks_[name]);
            }
        }
        for (auto& fn : fns) {
            if (fn) fn();
        }
        std::lock_guard<std::mutex> lock(mutex_);
        run_order_ = to_run;
        return to_run.size();
    }

    /// @brief Get the order in which initializers are/were registered.
    std::vector<std::string> getInitOrder() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return order_;
    }

    /// @brief Get the order in which initializers ran during the last runAll().
    std::vector<std::string> getRunOrder() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return run_order_;
    }

    /// @brief Number of registered initializers.
    std::size_t getInitCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return order_.size();
    }

    /// @brief Get utility statistics/status
    std::string getStatus() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return "DiagnosticsInitializer[registered=" + std::to_string(order_.size()) +
               ", last_run=" + std::to_string(run_order_.size()) + "]";
    }

    /// @brief Reset utility state
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        order_.clear();
        callbacks_.clear();
        run_order_.clear();
    }

private:
    DiagnosticsInitializer() = default;
    ~DiagnosticsInitializer() = default;

    DiagnosticsInitializer(const DiagnosticsInitializer&) = delete;
    DiagnosticsInitializer& operator=(const DiagnosticsInitializer&) = delete;
    DiagnosticsInitializer(DiagnosticsInitializer&&) = delete;
    DiagnosticsInitializer& operator=(DiagnosticsInitializer&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::vector<std::string> order_;
    std::map<std::string, InitFn> callbacks_;
    std::vector<std::string> run_order_;
};

} // namespace nexus::utility::meta
