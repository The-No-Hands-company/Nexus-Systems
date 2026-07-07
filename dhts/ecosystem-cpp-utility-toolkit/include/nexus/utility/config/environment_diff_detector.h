#pragma once

#include <string>
#include <map>
#include <vector>
#include <set>
#include <mutex>

namespace nexus::utility::config {

class EnvironmentDiffDetector {
public:
    struct DiffResult {
        std::vector<std::string> added;
        std::vector<std::string> removed;
        std::vector<std::string> changed;
    };

    static EnvironmentDiffDetector& instance() {
        static EnvironmentDiffDetector inst;
        return inst;
    }

    void initialize() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = true; }
    void shutdown() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = false; }
    bool isEnabled() const { return enabled_; }

    void setBaseline(const std::map<std::string, std::string>& env) {
        std::lock_guard<std::mutex> lk(mutex_);
        baseline_ = env;
    }

    DiffResult compare(const std::map<std::string, std::string>& env) const {
        std::lock_guard<std::mutex> lk(mutex_);
        DiffResult result;
        for (const auto& [k, v] : env) {
            auto it = baseline_.find(k);
            if (it == baseline_.end()) {
                result.added.push_back(k);
            } else if (it->second != v) {
                result.changed.push_back(k);
            }
        }
        for (const auto& [k, v] : baseline_) {
            if (env.find(k) == env.end()) {
                result.removed.push_back(k);
            }
        }
        return result;
    }

private:
    EnvironmentDiffDetector() = default;
    ~EnvironmentDiffDetector() = default;
    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::map<std::string, std::string> baseline_;
};

} // namespace nexus::utility::config
