#pragma once

#include <cstddef>
#include <functional>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace nexus::utility::safety {

/// @brief Registers safety invariants (predicates) and evaluates them for violations.
class SafetyInvariantChecker {
public:
    using Check = std::function<bool()>;

    static SafetyInvariantChecker& instance() {
        static SafetyInvariantChecker inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = true;
        config_ = config;
    }

    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
        invariants_.clear();
        last_violation_count_ = 0;
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    void addInvariant(const std::string& name, Check check) {
        std::lock_guard<std::mutex> lock(mutex_);
        invariants_.emplace_back(name, std::move(check));
    }

    /// @brief Evaluate all invariants; returns the names of those that were violated.
    std::vector<std::string> checkAll() {
        std::vector<std::pair<std::string, Check>> invariants;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            invariants = invariants_;
        }
        std::vector<std::string> violated;
        for (auto& [name, check] : invariants) {
            if (check && !check()) violated.push_back(name);
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            last_violation_count_ = violated.size();
        }
        return violated;
    }

    /// @brief Number of violations found in the most recent checkAll().
    size_t getViolationCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return last_violation_count_;
    }

    size_t getInvariantCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return invariants_.size();
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        invariants_.clear();
        last_violation_count_ = 0;
    }

private:
    SafetyInvariantChecker() = default;
    ~SafetyInvariantChecker() = default;
    SafetyInvariantChecker(const SafetyInvariantChecker&) = delete;
    SafetyInvariantChecker& operator=(const SafetyInvariantChecker&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::vector<std::pair<std::string, Check>> invariants_;
    size_t last_violation_count_ = 0;
};

} // namespace nexus::utility::safety
