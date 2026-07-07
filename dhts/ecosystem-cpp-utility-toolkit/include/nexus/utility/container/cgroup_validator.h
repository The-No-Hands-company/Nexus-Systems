#pragma once

#include <string>
#include <map>
#include <vector>
#include <mutex>
#include <cstdint>

namespace nexus::utility::container {

class CgroupValidator {
public:
    struct Violation {
        std::string cgroup;
        std::string param;
        int64_t limit;
        int64_t actual;
    };

    static CgroupValidator& instance() {
        static CgroupValidator inst;
        return inst;
    }

    void initialize(const std::string& config = "") { std::lock_guard<std::mutex> lk(mutex_); enabled_ = true; config_ = config; }
    void shutdown() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = false; }
    bool isEnabled() const { return enabled_; }
    void enable() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = true; }
    void disable() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = false; }
    void reset() { std::lock_guard<std::mutex> lk(mutex_); limits_.clear(); violations_.clear(); }

    void setLimit(const std::string& cgroup, const std::string& param, int64_t value) {
        std::lock_guard<std::mutex> lk(mutex_);
        limits_[cgroup][param] = value;
    }

    bool validateValue(const std::string& cgroup, const std::string& param, int64_t actual) {
        std::lock_guard<std::mutex> lk(mutex_);
        if (!enabled_) return true;
        auto it_cg = limits_.find(cgroup);
        if (it_cg == limits_.end()) return true;
        auto it_param = it_cg->second.find(param);
        if (it_param == it_cg->second.end()) return true;
        if (actual > it_param->second) {
            violations_.push_back({cgroup, param, it_param->second, actual});
            return false;
        }
        return true;
    }

    std::vector<Violation> getViolations() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return violations_;
    }

private:
    CgroupValidator() = default;
    ~CgroupValidator() = default;
    CgroupValidator(const CgroupValidator&) = delete;
    CgroupValidator& operator=(const CgroupValidator&) = delete;
    CgroupValidator(CgroupValidator&&) = delete;
    CgroupValidator& operator=(CgroupValidator&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::map<std::string, std::map<std::string, int64_t>> limits_;
    std::vector<Violation> violations_;
};

} // namespace nexus::utility::container
