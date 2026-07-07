#pragma once
#include <vector>

#include <string>
#include <set>
#include <map>
#include <mutex>

namespace nexus::utility::apivalidation {

class ReturnValueChecker {
public:
    static ReturnValueChecker& instance() {
        static ReturnValueChecker inst;
        return inst;
    }

    void initialize(const std::string& config = "") { std::lock_guard<std::mutex> lk(mutex_); enabled_ = true; config_ = config; }
    void shutdown() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = false; }
    bool isEnabled() const { return enabled_; }
    void enable() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = true; }
    void disable() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = false; }
    void reset() { std::lock_guard<std::mutex> lk(mutex_); nodiscard_funcs_.clear(); unchecked_.clear(); checked_.clear(); }

    void registerFunction(const std::string& name, bool nodiscard) {
        std::lock_guard<std::mutex> lk(mutex_);
        if (nodiscard) nodiscard_funcs_.insert(name);
    }

    void markChecked(const std::string& name) {
        std::lock_guard<std::mutex> lk(mutex_);
        if (!enabled_) return;
        checked_.insert(name);
    }

    void markCalled(const std::string& name) {
        std::lock_guard<std::mutex> lk(mutex_);
        if (!enabled_) return;
        if (nodiscard_funcs_.find(name) != nodiscard_funcs_.end()) {
            unchecked_.insert(name);
        }
    }

    std::vector<std::string> getUncheckedCalls() const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::vector<std::string> result;
        for (const auto& func : unchecked_) {
            if (checked_.find(func) == checked_.end()) {
                result.push_back(func);
            }
        }
        return result;
    }

private:
    ReturnValueChecker() = default;
    ~ReturnValueChecker() = default;
    ReturnValueChecker(const ReturnValueChecker&) = delete;
    ReturnValueChecker& operator=(const ReturnValueChecker&) = delete;
    ReturnValueChecker(ReturnValueChecker&&) = delete;
    ReturnValueChecker& operator=(ReturnValueChecker&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::set<std::string> nodiscard_funcs_;
    std::set<std::string> unchecked_;
    std::set<std::string> checked_;
};

} // namespace nexus::utility::apivalidation
