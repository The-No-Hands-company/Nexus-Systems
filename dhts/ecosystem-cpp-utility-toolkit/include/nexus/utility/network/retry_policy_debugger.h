#pragma once
#include <string>
#include <vector>
#include <chrono>
#include <unordered_map>
#include <mutex>
namespace nexus::utility::network {
class RetryPolicyDebugger {
public:
    struct RetryAttempt { std::string policyName; size_t attemptNumber; std::chrono::milliseconds delay; std::chrono::system_clock::time_point timestamp; bool success; };
    static RetryPolicyDebugger& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void registerPolicy(const std::string& name, size_t maxAttempts, std::chrono::milliseconds baseDelay);
    void recordAttempt(const std::string& policy, size_t attempt, std::chrono::milliseconds delay, bool success);
    std::vector<RetryAttempt> getHistory() const;
    size_t getTotalAttempts(const std::string& policy) const;
    double getSuccessRate(const std::string& policy) const;
    bool exceededMaxAttempts(const std::string& policy) const;
    void clear();
private:
    RetryPolicyDebugger() = default; ~RetryPolicyDebugger() = default; bool enabled_ = false;
    struct PolicyInfo { size_t maxAttempts; std::chrono::milliseconds baseDelay; size_t totalAttempts = 0; size_t successes = 0; };
    std::unordered_map<std::string, PolicyInfo> policies_;
    std::vector<RetryAttempt> history_;
    mutable std::mutex mutex_;
};
} // namespace nexus::utility::network