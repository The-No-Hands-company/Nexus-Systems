#pragma once

#include <cstddef>
#include <mutex>
#include <string>
#include <vector>

namespace nexus::utility::parser {

/// @brief Validates intermediate representation instructions and basic blocks.
class IrValidator {
public:
    static IrValidator& instance() {
        static IrValidator inst;
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
        violations_.clear();
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    /// @brief Validate an instruction has the expected operand count.
    /// @return true if valid; false (and records a violation) otherwise.
    bool validateInstruction(const std::string& opcode, int operand_count, int expected_count) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (operand_count != expected_count) {
            violations_.push_back("instruction '" + opcode + "' expected " +
                                  std::to_string(expected_count) + " operands, got " +
                                  std::to_string(operand_count));
            return false;
        }
        return true;
    }

    /// @brief Validate a basic block is properly terminated.
    /// @return true if valid; false (and records a violation) otherwise.
    bool validateBasicBlock(const std::string& name, bool has_terminator) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!has_terminator) {
            violations_.push_back("basic block '" + name + "' has no terminator");
            return false;
        }
        return true;
    }

    std::vector<std::string> getViolations() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return violations_;
    }

    size_t getViolationCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return violations_.size();
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        violations_.clear();
    }

private:
    IrValidator() = default;
    ~IrValidator() = default;
    IrValidator(const IrValidator&) = delete;
    IrValidator& operator=(const IrValidator&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::vector<std::string> violations_;
};

} // namespace nexus::utility::parser
