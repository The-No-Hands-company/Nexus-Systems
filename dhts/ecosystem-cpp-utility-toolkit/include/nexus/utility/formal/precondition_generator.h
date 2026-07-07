#pragma once

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <cstddef>

namespace nexus::utility::formal {

/**
 * @brief Generate `requires` preconditions for functions and validate calls.
 *
 * generateRequires() records a function's expected parameters and produces a
 * requires-clause string. validateCall() checks a call site against the recorded
 * signature (currently by argument arity), returning whether the call is valid.
 *
 * Thread safety: all public methods are protected by a mutex.
 *
 * @category Formal
 * @version 1.0.0
 */
class PreconditionGenerator {
public:
    /// @brief Get singleton instance
    static PreconditionGenerator& instance() {
        static PreconditionGenerator inst;
        return inst;
    }

    /// @brief Initialize the utility
    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = config;
        enabled_ = true;
        signatures_.clear();
        valid_calls_ = 0;
        invalid_calls_ = 0;
    }

    /// @brief Shutdown the utility and cleanup resources
    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
        signatures_.clear();
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

    /// @brief Record a function's parameters and generate a requires-clause.
    std::string generateRequires(const std::string& function,
                                 const std::vector<std::string>& params) {
        std::lock_guard<std::mutex> lock(mutex_);
        signatures_[function] = params;
        std::string out = "requires " + function + "(";
        for (std::size_t i = 0; i < params.size(); ++i) {
            out += params[i];
            if (i + 1 < params.size()) out += ", ";
        }
        out += "): ";
        if (params.empty()) {
            out += "true";
        } else {
            for (std::size_t i = 0; i < params.size(); ++i) {
                out += params[i] + " != invalid";
                if (i + 1 < params.size()) out += " && ";
            }
        }
        return out;
    }

    /// @brief Validate a call against a recorded signature. The call is valid if
    /// the function is known and the argument count matches the parameter count.
    bool validateCall(const std::string& function,
                      const std::vector<std::string>& args) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = signatures_.find(function);
        bool valid = it != signatures_.end() && it->second.size() == args.size();
        if (valid) {
            ++valid_calls_;
        } else {
            ++invalid_calls_;
        }
        return valid;
    }

    /// @brief Whether a signature has been generated for a function.
    bool hasSignature(const std::string& function) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return signatures_.find(function) != signatures_.end();
    }

    /// @brief Number of recorded function signatures.
    std::size_t getSignatureCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return signatures_.size();
    }

    /// @brief Number of calls that validated successfully.
    std::size_t getValidCallCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return valid_calls_;
    }

    /// @brief Number of calls that failed validation.
    std::size_t getInvalidCallCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return invalid_calls_;
    }

    /// @brief Get utility statistics/status
    std::string getStatus() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return "PreconditionGenerator[signatures=" + std::to_string(signatures_.size()) +
               ", valid=" + std::to_string(valid_calls_) +
               ", invalid=" + std::to_string(invalid_calls_) + "]";
    }

    /// @brief Reset utility state
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        signatures_.clear();
        valid_calls_ = 0;
        invalid_calls_ = 0;
    }

private:
    PreconditionGenerator() = default;
    ~PreconditionGenerator() = default;

    PreconditionGenerator(const PreconditionGenerator&) = delete;
    PreconditionGenerator& operator=(const PreconditionGenerator&) = delete;
    PreconditionGenerator(PreconditionGenerator&&) = delete;
    PreconditionGenerator& operator=(PreconditionGenerator&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::map<std::string, std::vector<std::string>> signatures_;
    std::size_t valid_calls_ = 0;
    std::size_t invalid_calls_ = 0;
};

} // namespace nexus::utility::formal
