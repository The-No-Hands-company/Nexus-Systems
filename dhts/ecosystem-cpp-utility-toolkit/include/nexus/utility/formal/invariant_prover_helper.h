#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <mutex>
#include <cstddef>

namespace nexus::utility::formal {

/**
 * @brief Register named invariants and prove (evaluate) them collectively.
 *
 * Each invariant is a predicate (function<bool()>). proveAll() evaluates every
 * invariant and reports how many passed and failed, keeping the names of any
 * that failed for diagnostics.
 *
 * Thread safety: all public methods are protected by a mutex.
 *
 * @category Formal
 * @version 1.0.0
 */
class InvariantProverHelper {
public:
    using Check = std::function<bool()>;

    struct ProofResult {
        std::size_t passed = 0;
        std::size_t failed = 0;
    };

    /// @brief Get singleton instance
    static InvariantProverHelper& instance() {
        static InvariantProverHelper inst;
        return inst;
    }

    /// @brief Initialize the utility
    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = config;
        enabled_ = true;
        invariants_.clear();
        order_.clear();
        failed_names_.clear();
    }

    /// @brief Shutdown the utility and cleanup resources
    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
        invariants_.clear();
        order_.clear();
        failed_names_.clear();
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

    /// @brief Register an invariant with a check predicate.
    void addInvariant(const std::string& name, Check check) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (invariants_.find(name) == invariants_.end()) {
            order_.push_back(name);
        }
        invariants_[name] = std::move(check);
    }

    /// @brief Evaluate all invariants. An invariant with no check predicate is
    /// treated as passing.
    /// @return {passed, failed} counts.
    ProofResult proveAll() {
        std::vector<std::string> names;
        std::vector<Check> checks;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto& name : order_) {
                names.push_back(name);
                checks.push_back(invariants_[name]);
            }
        }
        ProofResult result;
        std::vector<std::string> failed;
        for (std::size_t i = 0; i < names.size(); ++i) {
            bool ok = checks[i] ? checks[i]() : true;
            if (ok) {
                ++result.passed;
            } else {
                ++result.failed;
                failed.push_back(names[i]);
            }
        }
        std::lock_guard<std::mutex> lock(mutex_);
        failed_names_ = std::move(failed);
        return result;
    }

    /// @brief Names of invariants that failed during the last proveAll().
    std::vector<std::string> getFailedInvariants() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return failed_names_;
    }

    /// @brief Number of registered invariants.
    std::size_t getProofCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return invariants_.size();
    }

    /// @brief Get utility statistics/status
    std::string getStatus() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return "InvariantProverHelper[invariants=" + std::to_string(invariants_.size()) +
               ", last_failed=" + std::to_string(failed_names_.size()) + "]";
    }

    /// @brief Reset utility state
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        invariants_.clear();
        order_.clear();
        failed_names_.clear();
    }

private:
    InvariantProverHelper() = default;
    ~InvariantProverHelper() = default;

    InvariantProverHelper(const InvariantProverHelper&) = delete;
    InvariantProverHelper& operator=(const InvariantProverHelper&) = delete;
    InvariantProverHelper(InvariantProverHelper&&) = delete;
    InvariantProverHelper& operator=(InvariantProverHelper&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::map<std::string, Check> invariants_;
    std::vector<std::string> order_;
    std::vector<std::string> failed_names_;
};

} // namespace nexus::utility::formal
