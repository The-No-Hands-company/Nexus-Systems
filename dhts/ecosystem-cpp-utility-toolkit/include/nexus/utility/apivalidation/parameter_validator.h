#pragma once

#include <string>
#include <map>
#include <vector>
#include <mutex>
#include <utility>

namespace nexus::utility::apivalidation {

class ParameterValidator {
public:
    struct Violation {
        std::string func;
        std::string param;
        double value;
        double min;
        double max;
    };

    static ParameterValidator& instance() {
        static ParameterValidator inst;
        return inst;
    }

    void initialize(const std::string& config = "") { std::lock_guard<std::mutex> lk(mutex_); enabled_ = true; config_ = config; }
    void shutdown() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = false; }
    bool isEnabled() const { return enabled_; }
    void enable() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = true; }
    void disable() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = false; }
    void reset() { std::lock_guard<std::mutex> lk(mutex_); rules_.clear(); violations_.clear(); }

    void addRule(const std::string& func, const std::string& param, double min, double max) {
        std::lock_guard<std::mutex> lk(mutex_);
        rules_[func][param] = {min, max};
    }

    bool validate(const std::string& func, const std::string& param, double value) {
        std::lock_guard<std::mutex> lk(mutex_);
        if (!enabled_) return true;
        auto it_func = rules_.find(func);
        if (it_func == rules_.end()) return true;
        auto it_param = it_func->second.find(param);
        if (it_param == it_func->second.end()) return true;
        if (value < it_param->second.first || value > it_param->second.second) {
            violations_.push_back({func, param, value, it_param->second.first, it_param->second.second});
            return false;
        }
        return true;
    }

    std::vector<Violation> getViolations() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return violations_;
    }

private:
    ParameterValidator() = default;
    ~ParameterValidator() = default;
    ParameterValidator(const ParameterValidator&) = delete;
    ParameterValidator& operator=(const ParameterValidator&) = delete;
    ParameterValidator(ParameterValidator&&) = delete;
    ParameterValidator& operator=(ParameterValidator&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::map<std::string, std::map<std::string, std::pair<double, double>>> rules_;
    std::vector<Violation> violations_;
};

} // namespace nexus::utility::apivalidation
