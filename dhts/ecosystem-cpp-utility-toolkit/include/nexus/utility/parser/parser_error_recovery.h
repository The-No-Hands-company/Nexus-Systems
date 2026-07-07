#pragma once

#include <cstddef>
#include <mutex>
#include <string>
#include <vector>

namespace nexus::utility::parser {

/// @brief Tracks parser errors and the success rate of error recovery attempts.
class ParserErrorRecovery {
public:
    struct Error {
        size_t line = 0;
        size_t col = 0;
        std::string message;
    };

    static ParserErrorRecovery& instance() {
        static ParserErrorRecovery inst;
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
        recovery_attempts_ = 0;
        recovery_successes_ = 0;
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    void recordError(size_t line, size_t col, const std::string& message) {
        std::lock_guard<std::mutex> lock(mutex_);
        errors_.push_back({line, col, message});
    }

    void recordRecovery(bool success) {
        std::lock_guard<std::mutex> lock(mutex_);
        ++recovery_attempts_;
        if (success) ++recovery_successes_;
    }

    size_t getErrorCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return errors_.size();
    }

    /// @brief Fraction of recovery attempts that succeeded (0.0 if none attempted).
    double getRecoveryRate() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return recovery_attempts_ > 0
                   ? static_cast<double>(recovery_successes_) / static_cast<double>(recovery_attempts_)
                   : 0.0;
    }

    std::vector<Error> getErrors() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return errors_;
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        errors_.clear();
        recovery_attempts_ = 0;
        recovery_successes_ = 0;
    }

private:
    ParserErrorRecovery() = default;
    ~ParserErrorRecovery() = default;
    ParserErrorRecovery(const ParserErrorRecovery&) = delete;
    ParserErrorRecovery& operator=(const ParserErrorRecovery&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::vector<Error> errors_;
    size_t recovery_attempts_ = 0;
    size_t recovery_successes_ = 0;
};

} // namespace nexus::utility::parser
