#pragma once

#include <string>
#include <map>
#include <vector>
#include <mutex>

namespace nexus::utility::apivalidation {

class DeprecationWarner {
public:
    struct DeprecationInfo {
        std::string replacement;
        std::string version;
        std::string message;
    };

    static DeprecationWarner& instance() {
        static DeprecationWarner inst;
        return inst;
    }

    void initialize(const std::string& config = "") { std::lock_guard<std::mutex> lk(mutex_); enabled_ = true; config_ = config; }
    void shutdown() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = false; }
    bool isEnabled() const { return enabled_; }
    void enable() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = true; }
    void disable() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = false; }
    void reset() { std::lock_guard<std::mutex> lk(mutex_); deprecated_.clear(); calls_.clear(); }

    void markDeprecated(const std::string& func, const std::string& replacement, const std::string& version) {
        std::lock_guard<std::mutex> lk(mutex_);
        deprecated_[func] = {replacement, version, func + " is deprecated since " + version +
            (replacement.empty() ? "" : ", use " + replacement + " instead")};
    }

    bool isDeprecated(const std::string& func) const {
        std::lock_guard<std::mutex> lk(mutex_);
        return deprecated_.find(func) != deprecated_.end();
    }

    DeprecationInfo checkDeprecated(const std::string& func) {
        std::lock_guard<std::mutex> lk(mutex_);
        if (!enabled_) return {};
        calls_.push_back(func);
        auto it = deprecated_.find(func);
        return (it != deprecated_.end()) ? it->second : DeprecationInfo{};
    }

    std::vector<std::string> getDeprecatedCalls() const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::vector<std::string> result;
        for (const auto& call : calls_) {
            if (deprecated_.find(call) != deprecated_.end()) {
                result.push_back(call);
            }
        }
        return result;
    }

private:
    DeprecationWarner() = default;
    ~DeprecationWarner() = default;
    DeprecationWarner(const DeprecationWarner&) = delete;
    DeprecationWarner& operator=(const DeprecationWarner&) = delete;
    DeprecationWarner(DeprecationWarner&&) = delete;
    DeprecationWarner& operator=(DeprecationWarner&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::map<std::string, DeprecationInfo> deprecated_;
    std::vector<std::string> calls_;
};

} // namespace nexus::utility::apivalidation
