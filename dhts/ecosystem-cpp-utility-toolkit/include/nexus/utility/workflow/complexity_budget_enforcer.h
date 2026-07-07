#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>

namespace nexus::utility::workflow {

/**
 * @brief Enforce per-module complexity budgets.
 */
class ComplexityBudgetEnforcer {
public:
    struct Budget {
        std::string module;
        int budget = 0;
        int used = 0;
        int remaining() const { return budget - used; }
        bool exceeded() const { return used > budget; }
    };

    static ComplexityBudgetEnforcer& instance() {
        static ComplexityBudgetEnforcer inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    void setBudget(const std::string& module, int budget) {
        std::lock_guard<std::mutex> lk(mutex_);
        auto& b = budgets_[module];
        b.module = module;
        b.budget = budget;
    }

    /// Add complexity to a module; returns false if the budget is exceeded.
    bool addComplexity(const std::string& module, int complexity) {
        std::lock_guard<std::mutex> lk(mutex_);
        auto& b = budgets_[module];
        b.module = module;
        b.used += complexity;
        if (b.exceeded()) { ++violations_; return false; }
        return true;
    }

    Budget budget(const std::string& module) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = budgets_.find(module);
        return it == budgets_.end() ? Budget{module, 0, 0} : it->second;
    }

    std::vector<Budget> exceededModules() const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::vector<Budget> out;
        for (const auto& [m, b] : budgets_) if (b.exceeded()) out.push_back(b);
        return out;
    }

    std::size_t violations() const { return violations_.load(); }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        budgets_.clear();
        violations_ = 0;
    }

private:
    ComplexityBudgetEnforcer() = default;
    ~ComplexityBudgetEnforcer() = default;
    ComplexityBudgetEnforcer(const ComplexityBudgetEnforcer&) = delete;
    ComplexityBudgetEnforcer& operator=(const ComplexityBudgetEnforcer&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::unordered_map<std::string, Budget> budgets_;
    std::atomic<std::size_t> violations_{0};
};

} // namespace nexus::utility::workflow
