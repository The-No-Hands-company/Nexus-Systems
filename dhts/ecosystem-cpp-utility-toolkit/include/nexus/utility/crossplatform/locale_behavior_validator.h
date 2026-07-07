#pragma once

#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <cctype>

namespace nexus::utility::crossplatform {

/**
 * @brief Validate locale-specific behavior (decimal separators, case mapping).
 */
class LocaleBehaviorValidator {
public:
    struct Issue {
        std::string context;
        std::string description;
    };

    static LocaleBehaviorValidator& instance() {
        static LocaleBehaviorValidator inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    /// Verify that a numeric string parses to the expected value regardless of
    /// locale decimal separator ('.' vs ',').
    bool validateDecimalParsing(const std::string& input, double expected,
                                double tolerance = 1e-9) {
        std::string normalized;
        for (char c : input) normalized += (c == ',') ? '.' : c;
        double parsed = 0.0;
        try { parsed = std::stod(normalized); } catch (...) {
            recordIssue("decimal", "unparseable: " + input);
            return false;
        }
        bool ok = (parsed > expected ? parsed - expected : expected - parsed) <= tolerance;
        if (!ok) recordIssue("decimal", "mismatch parsing: " + input);
        return ok;
    }

    /// Detect the classic Turkish-i case-mapping hazard.
    bool hasCaseMappingHazard(const std::string& text) {
        for (char c : text)
            if (c == 'i' || c == 'I') return true;
        return false;
    }

    void recordIssue(const std::string& context, const std::string& description) {
        std::lock_guard<std::mutex> lk(mutex_);
        issues_.push_back({context, description});
    }

    std::vector<Issue> issues() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return issues_;
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        issues_.clear();
    }

private:
    LocaleBehaviorValidator() = default;
    ~LocaleBehaviorValidator() = default;
    LocaleBehaviorValidator(const LocaleBehaviorValidator&) = delete;
    LocaleBehaviorValidator& operator=(const LocaleBehaviorValidator&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::vector<Issue> issues_;
};

} // namespace nexus::utility::crossplatform
