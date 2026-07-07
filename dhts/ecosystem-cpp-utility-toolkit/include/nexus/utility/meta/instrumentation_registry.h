#pragma once

#include <string>
#include <vector>
#include <map>
#include <set>
#include <functional>
#include <mutex>
#include <cstddef>

namespace nexus::utility::meta {

/**
 * @brief Registry of instrumentation probes with paired start/stop callbacks.
 *
 * Each instrument is registered by name with a start and a stop callback.
 * startAll()/stopAll() invoke the corresponding callbacks and maintain a count
 * of currently-active instruments.
 *
 * Thread safety: all public methods are protected by a mutex.
 *
 * @category Meta
 * @version 1.0.0
 */
class InstrumentationRegistry {
public:
    using Callback = std::function<void()>;

    /// @brief Get singleton instance
    static InstrumentationRegistry& instance() {
        static InstrumentationRegistry inst;
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
        instruments_.clear();
        order_.clear();
        active_.clear();
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

    /// @brief Register an instrument with start and stop callbacks.
    void registerInstrument(const std::string& name, Callback start, Callback stop) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (instruments_.find(name) == instruments_.end()) {
            order_.push_back(name);
        }
        instruments_[name] = Entry{std::move(start), std::move(stop)};
    }

    /// @brief Start all registered instruments (in registration order).
    void startAll() {
        std::vector<Callback> starts;
        std::vector<std::string> names;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!enabled_) return;
            for (const auto& name : order_) {
                starts.push_back(instruments_[name].start);
                names.push_back(name);
            }
        }
        for (std::size_t i = 0; i < starts.size(); ++i) {
            if (starts[i]) starts[i]();
        }
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& name : names) {
            active_.insert(name);
        }
    }

    /// @brief Stop all registered instruments (in registration order).
    void stopAll() {
        std::vector<Callback> stops;
        std::vector<std::string> names;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto& name : order_) {
                stops.push_back(instruments_[name].stop);
                names.push_back(name);
            }
        }
        for (std::size_t i = 0; i < stops.size(); ++i) {
            if (stops[i]) stops[i]();
        }
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& name : names) {
            active_.erase(name);
        }
    }

    /// @brief Number of currently-active instruments.
    std::size_t getActiveCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return active_.size();
    }

    /// @brief Number of registered instruments.
    std::size_t getInstrumentCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return instruments_.size();
    }

    /// @brief Get utility statistics/status
    std::string getStatus() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return "InstrumentationRegistry[registered=" + std::to_string(instruments_.size()) +
               ", active=" + std::to_string(active_.size()) + "]";
    }

    /// @brief Reset utility state
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        instruments_.clear();
        order_.clear();
        active_.clear();
    }

private:
    struct Entry {
        Callback start;
        Callback stop;
    };

    InstrumentationRegistry() = default;
    ~InstrumentationRegistry() = default;

    InstrumentationRegistry(const InstrumentationRegistry&) = delete;
    InstrumentationRegistry& operator=(const InstrumentationRegistry&) = delete;
    InstrumentationRegistry(InstrumentationRegistry&&) = delete;
    InstrumentationRegistry& operator=(InstrumentationRegistry&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::map<std::string, Entry> instruments_;
    std::vector<std::string> order_;
    std::set<std::string> active_;
};

} // namespace nexus::utility::meta
