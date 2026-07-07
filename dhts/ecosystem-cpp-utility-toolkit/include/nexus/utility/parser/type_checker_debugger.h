#pragma once

#include <cstddef>
#include <mutex>
#include <string>
#include <vector>

namespace nexus::utility::parser {

/// @brief Debugs type checking by recording checks and reporting type mismatches.
class TypeCheckerDebugger {
public:
    struct TypeError {
        std::string expr;
        std::string expected;
        std::string actual;
    };

    static TypeCheckerDebugger& instance() {
        static TypeCheckerDebugger inst;
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
        errors_.clear();
        total_checks_ = 0;
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    void recordTypeCheck(const std::string& expr, const std::string& expected,
                         const std::string& actual, bool matched) {
        std::lock_guard<std::mutex> lock(mutex_);
        ++total_checks_;
        if (!matched) {
            errors_.push_back({expr, expected, actual});
        }
    }

    std::vector<TypeError> getTypeErrors() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return errors_;
    }

    size_t getErrorCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return errors_.size();
    }

    /// @brief Fraction of type checks that failed (0.0 if none performed).
    double getErrorRate() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return total_checks_ > 0
                   ? static_cast<double>(errors_.size()) / static_cast<double>(total_checks_)
                   : 0.0;
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        errors_.clear();
        total_checks_ = 0;
    }

private:
    TypeCheckerDebugger() = default;
    ~TypeCheckerDebugger() = default;
    TypeCheckerDebugger(const TypeCheckerDebugger&) = delete;
    TypeCheckerDebugger& operator=(const TypeCheckerDebugger&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::vector<TypeError> errors_;
    size_t total_checks_ = 0;
};

} // namespace nexus::utility::parser
