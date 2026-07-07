#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <cstddef>

namespace nexus::utility::allocator {

/**
 * @brief Validate arena allocator state and detect misuse.
 *
 * Checks the fundamental arena invariant (used <= capacity), tracks the number
 * of validations performed, and provides a simple fragmentation estimate based
 * on the ratio of used to capacity across recorded validations.
 *
 * Thread safety: all public methods are protected by a mutex.
 *
 * @category Allocator
 * @version 1.0.0
 */
class ArenaValidator {
public:
    struct ValidationResult {
        bool valid = false;
        std::size_t capacity = 0;
        std::size_t used = 0;
        std::size_t wasted = 0;
    };

    /// @brief Get singleton instance
    static ArenaValidator& instance() {
        static ArenaValidator inst;
        return inst;
    }

    /// @brief Initialize the utility
    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = config;
        enabled_ = true;
        validation_count_ = 0;
        failure_count_ = 0;
        results_.clear();
    }

    /// @brief Shutdown the utility and cleanup resources
    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
        results_.clear();
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

    /// @brief Validate an arena: the used bytes must not exceed capacity.
    /// @return true if the arena is in a valid state.
    bool validateArena(std::size_t capacity, std::size_t used) {
        std::lock_guard<std::mutex> lock(mutex_);
        ++validation_count_;
        bool valid = used <= capacity;
        if (!valid) ++failure_count_;
        results_.push_back({valid, capacity, used, valid ? capacity - used : 0});
        return valid;
    }

    /// @brief Estimate fragmentation as the fraction of wasted (unused) capacity
    /// across all recorded validations. Returns a value in [0.0, 1.0].
    double checkFragmentation() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::size_t total_capacity = 0;
        std::size_t total_used = 0;
        for (const auto& r : results_) {
            if (!r.valid) continue;
            total_capacity += r.capacity;
            total_used += r.used;
        }
        if (total_capacity == 0) return 0.0;
        return static_cast<double>(total_capacity - total_used) /
               static_cast<double>(total_capacity);
    }

    /// @brief Number of validations performed.
    std::size_t getValidationCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return validation_count_;
    }

    /// @brief Number of validations that failed.
    std::size_t getFailureCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return failure_count_;
    }

    /// @brief Get utility statistics/status
    std::string getStatus() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return "ArenaValidator[validations=" + std::to_string(validation_count_) +
               ", failures=" + std::to_string(failure_count_) + "]";
    }

    /// @brief Reset utility state
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        validation_count_ = 0;
        failure_count_ = 0;
        results_.clear();
    }

private:
    ArenaValidator() = default;
    ~ArenaValidator() = default;

    ArenaValidator(const ArenaValidator&) = delete;
    ArenaValidator& operator=(const ArenaValidator&) = delete;
    ArenaValidator(ArenaValidator&&) = delete;
    ArenaValidator& operator=(ArenaValidator&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::size_t validation_count_ = 0;
    std::size_t failure_count_ = 0;
    std::vector<ValidationResult> results_;
};

} // namespace nexus::utility::allocator
