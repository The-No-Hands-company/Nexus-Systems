#pragma once
#include <string>
#include <vector>
#include <chrono>
#include <functional>
namespace nexus::utility::testing {
class TestTimeout {
public:
    struct TimeoutResult { std::string testName; std::chrono::milliseconds timeout; std::chrono::milliseconds actualDuration; bool timedOut; };
    static TestTimeout& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    TimeoutResult runWithTimeout(const std::string& name, std::function<void()> test, std::chrono::milliseconds timeout=std::chrono::milliseconds(5000));
    std::vector<TimeoutResult> getResults() const;
    size_t getTimeoutCount() const;
    void setDefaultTimeout(std::chrono::milliseconds timeout);
    void clear();
private:
    TestTimeout()=default; ~TestTimeout()=default; bool enabled_=false;
    std::chrono::milliseconds defaultTimeout_{5000};
    std::vector<TimeoutResult> results_;
};
} // namespace nexus::utility::testing