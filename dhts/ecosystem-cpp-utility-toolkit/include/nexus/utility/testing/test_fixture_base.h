#pragma once
#include <string>
#include <vector>
#include <chrono>
#include <functional>
#include <unordered_map>
namespace nexus::utility::testing {
class TestFixtureBase {
public:
    struct TestResult { std::string name; bool passed; std::chrono::microseconds duration{0}; std::string error; };
    using TestFn = std::function<void()>;
    static TestFixtureBase& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void registerTest(const std::string& suite, const std::string& name, TestFn fn);
    void registerSetup(const std::string& suite, TestFn fn);
    void registerTeardown(const std::string& suite, TestFn fn);
    std::vector<TestResult> runSuite(const std::string& suite);
    std::vector<TestResult> runAll();
    size_t getPassedCount() const;
    size_t getFailedCount() const;
    std::vector<TestResult> getResults() const;
    void clear();
private:
    TestFixtureBase()=default; ~TestFixtureBase()=default; bool enabled_=false;
    struct Suite { std::vector<std::pair<std::string,TestFn>> tests; TestFn setup; TestFn teardown; };
    std::unordered_map<std::string,Suite> suites_;
    std::vector<TestResult> results_;
};
} // namespace nexus::utility::testing