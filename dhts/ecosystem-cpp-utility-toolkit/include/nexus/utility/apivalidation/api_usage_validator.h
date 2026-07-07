#pragma once

#include <string>
#include <vector>
#include <map>
#include <set>
#include <mutex>

namespace nexus::utility::apivalidation {

class ApiUsageValidator {
public:
    static ApiUsageValidator& instance() {
        static ApiUsageValidator inst;
        return inst;
    }

    void initialize(const std::string& config = "") { std::lock_guard<std::mutex> lk(mutex_); enabled_ = true; config_ = config; }
    void shutdown() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = false; }
    bool isEnabled() const { return enabled_; }
    void enable() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = true; }
    void disable() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = false; }
    void reset() { std::lock_guard<std::mutex> lk(mutex_); apis_.clear(); called_.clear(); }

    void registerApi(const std::string& name, const std::vector<std::string>& requiredCalls) {
        std::lock_guard<std::mutex> lk(mutex_);
        apis_[name] = std::set<std::string>(requiredCalls.begin(), requiredCalls.end());
    }

    void recordCalled(const std::string& api) {
        std::lock_guard<std::mutex> lk(mutex_);
        if (!enabled_) return;
        called_.insert(api);
    }

    bool checkCompliance(const std::string& name) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = apis_.find(name);
        if (it == apis_.end()) return false;
        for (const auto& required : it->second) {
            if (called_.find(required) == called_.end()) return false;
        }
        return true;
    }

private:
    ApiUsageValidator() = default;
    ~ApiUsageValidator() = default;
    ApiUsageValidator(const ApiUsageValidator&) = delete;
    ApiUsageValidator& operator=(const ApiUsageValidator&) = delete;
    ApiUsageValidator(ApiUsageValidator&&) = delete;
    ApiUsageValidator& operator=(ApiUsageValidator&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::map<std::string, std::set<std::string>> apis_;
    std::set<std::string> called_;
};

} // namespace nexus::utility::apivalidation
