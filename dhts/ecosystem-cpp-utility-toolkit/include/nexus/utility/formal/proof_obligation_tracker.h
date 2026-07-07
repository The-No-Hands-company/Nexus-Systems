#pragma once

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <cstddef>

namespace nexus::utility::formal {

/**
 * @brief Track proof obligations through their lifecycle.
 *
 * Each obligation moves through states: pending -> proven -> discharged. Adding
 * an obligation records its description. proveObligation() marks that a proof was
 * found; dischargeObligation() marks it as fully discharged (closed).
 *
 * Thread safety: all public methods are protected by a mutex.
 *
 * @category Formal
 * @version 1.0.0
 */
class ProofObligationTracker {
public:
    struct Obligation {
        std::string description;
        bool proven = false;
        bool discharged = false;
    };

    /// @brief Get singleton instance
    static ProofObligationTracker& instance() {
        static ProofObligationTracker inst;
        return inst;
    }

    /// @brief Initialize the utility
    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = config;
        enabled_ = true;
        obligations_.clear();
        order_.clear();
    }

    /// @brief Shutdown the utility and cleanup resources
    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
        obligations_.clear();
        order_.clear();
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

    /// @brief Add a proof obligation with a description (pending state).
    void addObligation(const std::string& name, const std::string& description) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (obligations_.find(name) == obligations_.end()) {
            order_.push_back(name);
        }
        obligations_[name] = Obligation{description, false, false};
    }

    /// @brief Mark an obligation as proven. Returns false if unknown.
    bool proveObligation(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = obligations_.find(name);
        if (it == obligations_.end()) return false;
        it->second.proven = true;
        return true;
    }

    /// @brief Mark an obligation as discharged (closed). An obligation must be
    /// proven before it can be discharged. Returns false if unknown or not yet
    /// proven.
    bool dischargeObligation(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = obligations_.find(name);
        if (it == obligations_.end() || !it->second.proven) return false;
        it->second.discharged = true;
        return true;
    }

    /// @brief Whether an obligation is proven.
    bool isProven(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = obligations_.find(name);
        return it != obligations_.end() && it->second.proven;
    }

    /// @brief Whether an obligation is discharged.
    bool isDischarged(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = obligations_.find(name);
        return it != obligations_.end() && it->second.discharged;
    }

    /// @brief Number of discharged obligations.
    std::size_t getDischargedCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::size_t count = 0;
        for (const auto& [name, o] : obligations_) {
            if (o.discharged) ++count;
        }
        return count;
    }

    /// @brief Number of proven obligations.
    std::size_t getProvenCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::size_t count = 0;
        for (const auto& [name, o] : obligations_) {
            if (o.proven) ++count;
        }
        return count;
    }

    /// @brief Total number of obligations.
    std::size_t getTotalCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return obligations_.size();
    }

    /// @brief Get utility statistics/status
    std::string getStatus() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::size_t discharged = 0;
        for (const auto& [name, o] : obligations_) {
            if (o.discharged) ++discharged;
        }
        return "ProofObligationTracker[total=" + std::to_string(obligations_.size()) +
               ", discharged=" + std::to_string(discharged) + "]";
    }

    /// @brief Reset utility state
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        obligations_.clear();
        order_.clear();
    }

private:
    ProofObligationTracker() = default;
    ~ProofObligationTracker() = default;

    ProofObligationTracker(const ProofObligationTracker&) = delete;
    ProofObligationTracker& operator=(const ProofObligationTracker&) = delete;
    ProofObligationTracker(ProofObligationTracker&&) = delete;
    ProofObligationTracker& operator=(ProofObligationTracker&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::map<std::string, Obligation> obligations_;
    std::vector<std::string> order_;
};

} // namespace nexus::utility::formal
