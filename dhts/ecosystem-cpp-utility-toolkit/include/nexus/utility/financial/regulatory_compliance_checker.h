#pragma once

#include <functional>
#include <mutex>
#include <string>
#include <vector>
#include <utility>

namespace nexus::utility::financial {

class RegulatoryComplianceChecker {
public:
    using RuleCheckFn = std::function<bool(const std::string&)>;

    static RegulatoryComplianceChecker& instance() {
        static RegulatoryComplianceChecker inst;
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
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    void enable() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = true;
    }

    void disable() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
    }

    std::string getStatus() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!enabled_) return "RegulatoryComplianceChecker disabled";
        return "RegulatoryComplianceChecker active, rules=" + std::to_string(rules_.size())
             + ", violations=" + std::to_string(violations_.size());
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        rules_.clear();
        violations_.clear();
    }

    void addRule(const std::string& rule_name, RuleCheckFn check_fn) {
        std::lock_guard<std::mutex> lock(mutex_);
        rules_.emplace_back(rule_name, std::move(check_fn));
    }

    std::vector<std::string> checkCompliance(const std::string& transaction_data) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!enabled_) return {};
        violations_.clear();
        for (const auto& rule : rules_) {
            if (!rule.second(transaction_data)) {
                violations_.push_back(rule.first);
            }
        }
        return violations_;
    }

    std::vector<std::string> getViolations() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return violations_;
    }

private:
    RegulatoryComplianceChecker() = default;
    ~RegulatoryComplianceChecker() = default;

    RegulatoryComplianceChecker(const RegulatoryComplianceChecker&) = delete;
    RegulatoryComplianceChecker& operator=(const RegulatoryComplianceChecker&) = delete;
    RegulatoryComplianceChecker(RegulatoryComplianceChecker&&) = delete;
    RegulatoryComplianceChecker& operator=(RegulatoryComplianceChecker&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::vector<std::pair<std::string, RuleCheckFn>> rules_;
    std::vector<std::string> violations_;
};

} // namespace nexus::utility::financial
