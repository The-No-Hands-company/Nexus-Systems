#pragma once

#include <functional>
#include <chrono>
#include <string>
#include <vector>
#include <stdexcept>
#include "nexus/utility/scheduling/backoff_strategy.h"

namespace nexus::utility::scheduling {

/// Configurable retry policy with backoff
class RetryPolicy {
public:
    struct Config {
        int max_attempts = 3;
        BackoffStrategy::Config backoff;
        std::vector<std::string> retryable_errors;  // error message substrings
    };

    template<typename Func>
    auto execute(Func&& fn) -> decltype(fn()) {
        Config config;
        return executeWith(config, std::forward<Func>(fn));
    }

    template<typename Func>
    auto executeWith(const Config& config, Func&& fn) -> decltype(fn()) {
        BackoffStrategy backoff(config.backoff);
        std::string last_error;

        for (int attempt = 0; attempt < config.max_attempts; ++attempt) {
            try {
                return fn();
            } catch (const std::exception& e) {
                last_error = e.what();
                if (attempt == config.max_attempts - 1) {
                    throw;  // Last attempt, rethrow
                }
                if (!isRetryable(last_error, config.retryable_errors)) {
                    throw;  // Non-retryable error
                }
                backoff.wait();
            } catch (...) {
                if (attempt == config.max_attempts - 1) throw;
                backoff.wait();
            }
        }

        throw std::runtime_error("RetryPolicy: max attempts reached: " + last_error);
    }

private:
    static bool isRetryable(const std::string& error,
                             const std::vector<std::string>& retryable) {
        if (retryable.empty()) return true; // All errors retryable by default
        for (const auto& pattern : retryable) {
            if (error.find(pattern) != std::string::npos) return true;
        }
        return false;
    }
};

} // namespace nexus::utility::scheduling
