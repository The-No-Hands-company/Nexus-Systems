#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>

namespace nexus::utility::parser {

/// @brief Visualizes register allocation: variable/register assignments, pressure and spills.
class RegisterAllocatorVisualizer {
public:
    static RegisterAllocatorVisualizer& instance() {
        static RegisterAllocatorVisualizer inst;
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
        var_to_reg_.clear();
        reg_to_var_.clear();
        spill_count_ = 0;
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    /// @brief Assign a register to a variable. If the register is already held by a
    ///        different variable, that prior occupant is spilled.
    void allocateRegister(const std::string& variable, uint8_t reg) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto occupant = reg_to_var_.find(reg);
        if (occupant != reg_to_var_.end() && occupant->second != variable) {
            ++spill_count_;
            var_to_reg_.erase(occupant->second);
        }
        auto prev = var_to_reg_.find(variable);
        if (prev != var_to_reg_.end() && prev->second != reg) {
            reg_to_var_.erase(prev->second);
        }
        var_to_reg_[variable] = reg;
        reg_to_var_[reg] = variable;
    }

    void freeRegister(uint8_t reg) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = reg_to_var_.find(reg);
        if (it != reg_to_var_.end()) {
            var_to_reg_.erase(it->second);
            reg_to_var_.erase(it);
        }
    }

    /// @brief Get the register assigned to a variable, or -1 if unassigned.
    int getRegisterAssignment(const std::string& variable) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = var_to_reg_.find(variable);
        return it != var_to_reg_.end() ? static_cast<int>(it->second) : -1;
    }

    size_t getSpillCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return spill_count_;
    }

    /// @brief Current register pressure = number of registers in use.
    size_t getRegisterPressure() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return reg_to_var_.size();
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        var_to_reg_.clear();
        reg_to_var_.clear();
        spill_count_ = 0;
    }

private:
    RegisterAllocatorVisualizer() = default;
    ~RegisterAllocatorVisualizer() = default;
    RegisterAllocatorVisualizer(const RegisterAllocatorVisualizer&) = delete;
    RegisterAllocatorVisualizer& operator=(const RegisterAllocatorVisualizer&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::map<std::string, uint8_t> var_to_reg_;
    std::map<uint8_t, std::string> reg_to_var_;
    size_t spill_count_ = 0;
};

} // namespace nexus::utility::parser
