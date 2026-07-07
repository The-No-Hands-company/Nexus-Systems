#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <cstddef>

namespace nexus::utility::formal {

/**
 * @brief Track a loop's bounds and verify its invariant across iterations.
 *
 * setLoop() establishes the loop name and its [start, end] bounds and resets the
 * iteration counter to `start`. recordIteration() advances the counter.
 * checkInvariant() records a candidate invariant predicate and verifies the
 * fundamental bound invariant (start <= current index <= end).
 *
 * Thread safety: all public methods are protected by a mutex.
 *
 * @category Formal
 * @version 1.0.0
 */
class LoopInvariantHelper {
public:
    /// @brief Get singleton instance
    static LoopInvariantHelper& instance() {
        static LoopInvariantHelper inst;
        return inst;
    }

    /// @brief Initialize the utility
    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = config;
        enabled_ = true;
        loop_name_.clear();
        start_ = 0;
        end_ = 0;
        current_ = 0;
        iterations_ = 0;
        predicates_.clear();
    }

    /// @brief Shutdown the utility and cleanup resources
    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
        predicates_.clear();
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

    /// @brief Define the current loop and its bounds. Resets iteration state.
    void setLoop(const std::string& name, int start, int end) {
        std::lock_guard<std::mutex> lock(mutex_);
        loop_name_ = name;
        start_ = start;
        end_ = end;
        current_ = start;
        iterations_ = 0;
        predicates_.clear();
    }

    /// @brief Record an invariant predicate and check the fundamental loop-bound
    /// invariant: start <= current index <= end.
    /// @return true if the bound invariant currently holds.
    bool checkInvariant(const std::string& predicate) {
        std::lock_guard<std::mutex> lock(mutex_);
        predicates_.push_back(predicate);
        return current_ >= start_ && current_ <= end_;
    }

    /// @brief Advance the loop by one iteration (increments the current index).
    void recordIteration() {
        std::lock_guard<std::mutex> lock(mutex_);
        ++current_;
        ++iterations_;
    }

    /// @brief Number of iterations recorded.
    std::size_t getIterationCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return iterations_;
    }

    /// @brief Current loop index.
    int getCurrentIndex() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return current_;
    }

    /// @brief Predicates recorded via checkInvariant for the current loop.
    std::vector<std::string> getPredicates() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return predicates_;
    }

    /// @brief Name of the current loop.
    std::string getLoopName() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return loop_name_;
    }

    /// @brief Get utility statistics/status
    std::string getStatus() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return "LoopInvariantHelper[loop=" + loop_name_ +
               ", index=" + std::to_string(current_) +
               ", iterations=" + std::to_string(iterations_) + "]";
    }

    /// @brief Reset utility state
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        loop_name_.clear();
        start_ = 0;
        end_ = 0;
        current_ = 0;
        iterations_ = 0;
        predicates_.clear();
    }

private:
    LoopInvariantHelper() = default;
    ~LoopInvariantHelper() = default;

    LoopInvariantHelper(const LoopInvariantHelper&) = delete;
    LoopInvariantHelper& operator=(const LoopInvariantHelper&) = delete;
    LoopInvariantHelper(LoopInvariantHelper&&) = delete;
    LoopInvariantHelper& operator=(LoopInvariantHelper&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::string loop_name_;
    int start_ = 0;
    int end_ = 0;
    int current_ = 0;
    std::size_t iterations_ = 0;
    std::vector<std::string> predicates_;
};

} // namespace nexus::utility::formal
