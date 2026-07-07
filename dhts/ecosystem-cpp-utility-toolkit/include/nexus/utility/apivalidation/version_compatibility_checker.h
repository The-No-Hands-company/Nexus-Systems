#pragma once

#include <string>
#include <map>
#include <vector>
#include <mutex>
#include <tuple>

namespace nexus::utility::apivalidation {

class VersionCompatibilityChecker {
public:
    using SemVer = std::tuple<int, int, int>;

    static VersionCompatibilityChecker& instance() {
        static VersionCompatibilityChecker inst;
        return inst;
    }

    void initialize(const std::string& config = "") { std::lock_guard<std::mutex> lk(mutex_); enabled_ = true; config_ = config; }
    void shutdown() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = false; }
    bool isEnabled() const { return enabled_; }
    void enable() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = true; }
    void disable() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = false; }
    void reset() { std::lock_guard<std::mutex> lk(mutex_); versions_.clear(); incompatible_.clear(); }

    void registerApi(const std::string& name, const std::string& version) {
        std::lock_guard<std::mutex> lk(mutex_);
        versions_[name] = parseVersion(version);
    }

    bool checkCompat(const std::string& name, const std::string& requiredVersion) {
        std::lock_guard<std::mutex> lk(mutex_);
        if (!enabled_) return true;
        auto it = versions_.find(name);
        if (it == versions_.end()) return false;
        auto required = parseVersion(requiredVersion);
        bool compat = it->second >= required;
        if (!compat) incompatible_.push_back(name);
        return compat;
    }

    std::vector<std::string> getIncompatibleVersions() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return incompatible_;
    }

private:
    static SemVer parseVersion(const std::string& version) {
        int major = 0, minor = 0, patch = 0;
        size_t p1 = version.find('.');
        size_t p2 = (p1 != std::string::npos) ? version.find('.', p1 + 1) : std::string::npos;
        major = std::stoi(version.substr(0, p1));
        minor = std::stoi(version.substr(p1 + 1, p2 - p1 - 1));
        patch = (p2 != std::string::npos) ? std::stoi(version.substr(p2 + 1)) : 0;
        return {major, minor, patch};
    }

    VersionCompatibilityChecker() = default;
    ~VersionCompatibilityChecker() = default;
    VersionCompatibilityChecker(const VersionCompatibilityChecker&) = delete;
    VersionCompatibilityChecker& operator=(const VersionCompatibilityChecker&) = delete;
    VersionCompatibilityChecker(VersionCompatibilityChecker&&) = delete;
    VersionCompatibilityChecker& operator=(VersionCompatibilityChecker&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::map<std::string, SemVer> versions_;
    std::vector<std::string> incompatible_;
};

} // namespace nexus::utility::apivalidation
