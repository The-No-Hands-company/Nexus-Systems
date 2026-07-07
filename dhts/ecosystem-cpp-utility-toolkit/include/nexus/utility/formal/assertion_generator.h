#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <cstddef>

namespace nexus::utility::formal {

/**
 * @brief Auto-generate runtime assertion / contract expressions as strings.
 *
 * Produces precondition, postcondition and loop-invariant assertion text from
 * structured inputs. The generated strings can be emitted into source or logs
 * to document and check contracts.
 *
 * Thread safety: all public methods are protected by a mutex.
 *
 * @category Formal
 * @version 1.0.0
 */
class AssertionGenerator {
public:
    /// @brief Get singleton instance
    static AssertionGenerator& instance() {
        static AssertionGenerator inst;
        return inst;
    }

    /// @brief Initialize the utility
    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = config;
        enabled_ = true;
        generated_count_ = 0;
    }

    /// @brief Shutdown the utility and cleanup resources
    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
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

    /// @brief Generate a precondition assertion for a function: each parameter
    /// is asserted to be valid (non-null pointer style check).
    std::string generatePrecondition(const std::string& function_name,
                                     const std::vector<std::string>& params) {
        std::lock_guard<std::mutex> lock(mutex_);
        ++generated_count_;
        std::string out = "// precondition of " + function_name + "\n";
        if (params.empty()) {
            out += "assert(true); // no parameters";
            return out;
        }
        for (std::size_t i = 0; i < params.size(); ++i) {
            out += "assert(" + params[i] + " is valid);";
            if (i + 1 < params.size()) out += "\n";
        }
        return out;
    }

    /// @brief Generate a postcondition assertion tying the result to a return type.
    std::string generatePostcondition(const std::string& function_name,
                                      const std::string& return_type) {
        std::lock_guard<std::mutex> lock(mutex_);
        ++generated_count_;
        std::string out = "// postcondition of " + function_name + "\n";
        if (return_type == "void") {
            out += "assert(true); // void return";
        } else {
            out += "assert(result is a valid " + return_type + ");";
        }
        return out;
    }

    /// @brief Generate a loop invariant asserting the loop variable stays within
    /// the given range (formatted as "low..high").
    std::string generateInvariant(const std::string& loop_var,
                                  const std::string& range) {
        std::lock_guard<std::mutex> lock(mutex_);
        ++generated_count_;
        auto dots = range.find("..");
        std::string out = "// loop invariant\n";
        if (dots == std::string::npos) {
            out += "assert(" + loop_var + " in " + range + ");";
        } else {
            std::string low = range.substr(0, dots);
            std::string high = range.substr(dots + 2);
            out += "assert(" + low + " <= " + loop_var + " && " + loop_var +
                   " <= " + high + ");";
        }
        return out;
    }

    /// @brief Number of assertions generated.
    std::size_t getGeneratedCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return generated_count_;
    }

    /// @brief Get utility statistics/status
    std::string getStatus() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return "AssertionGenerator[generated=" + std::to_string(generated_count_) + "]";
    }

    /// @brief Reset utility state
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        generated_count_ = 0;
    }

private:
    AssertionGenerator() = default;
    ~AssertionGenerator() = default;

    AssertionGenerator(const AssertionGenerator&) = delete;
    AssertionGenerator& operator=(const AssertionGenerator&) = delete;
    AssertionGenerator(AssertionGenerator&&) = delete;
    AssertionGenerator& operator=(AssertionGenerator&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::size_t generated_count_ = 0;
};

} // namespace nexus::utility::formal
