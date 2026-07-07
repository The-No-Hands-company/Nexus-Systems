#pragma once

#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include <mutex>

namespace nexus::utility::workflow {

/**
 * @brief Run and track backward-compatibility test cases.
 */
class BackwardsCompatibilityTester {
public:
    using TestFn = std::function<bool()>;

    struct TestResult {
        std::string name;
        std::string sinceVersion;
        bool passed = false;
    };

    static BackwardsCompatibilityTester& instance() {
        static BackwardsCompatibilityTester inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    /// Register a compatibility test asserting old behavior still holds.
    void addTest(const std::string& name, const std::string& sinceVersion, TestFn fn) {
        std::lock_guard<std::mutex> lk(mutex_);
        tests_.push_back({name, sinceVersion, std::move(fn)});
    }

    /// Run all registered tests and record results.
    std::vector<TestResult> runAll() {
        std::vector<Test> local;
        { std::lock_guard<std::mutex> lk(mutex_); local = tests_; }
        std::vector<TestResult> results;
        for (auto& t : local) {
            bool passed = false;
            try { passed = t.fn ? t.fn() : false; } catch (...) { passed = false; }
            results.push_back({t.name, t.sinceVersion, passed});
        }
        std::lock_guard<std::mutex> lk(mutex_);
        results_ = results;
        return results;
    }

    std::vector<TestResult> failures() const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::vector<TestResult> out;
        for (const auto& r : results_) if (!r.passed) out.push_back(r);
        return out;
    }

    bool allPassed() const {
        std::lock_guard<std::mutex> lk(mutex_);
        for (const auto& r : results_) if (!r.passed) return false;
        return true;
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        tests_.clear();
        results_.clear();
    }

private:
    BackwardsCompatibilityTester() = default;
    ~BackwardsCompatibilityTester() = default;
    BackwardsCompatibilityTester(const BackwardsCompatibilityTester&) = delete;
    BackwardsCompatibilityTester& operator=(const BackwardsCompatibilityTester&) = delete;

    struct Test {
        std::string name;
        std::string sinceVersion;
        TestFn fn;
    };

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::vector<Test> tests_;
    std::vector<TestResult> results_;
};

} // namespace nexus::utility::workflow
