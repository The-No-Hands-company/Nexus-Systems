#pragma once
#include <string>
#include <vector>
#include <unordered_set>
#include <source_location>
#include <unordered_map>

namespace nexus::utility::cpp26 {

/// Detect abandoned/leaked coroutines (C++26 symmetric transfer + as_tuple awareness)
class CoroutineLeakDetector {
public:
    struct LeakReport {
        std::string coroutineName;
        std::source_location creationSite;
        size_t suspendCount;
        bool wasDestroyed;
        bool isLeaked;
    };

    void trackCreation(const std::string& name,
                       std::source_location loc = std::source_location::current()) {
        active_[name] = {name, loc, 0, false, false};
    }

    void trackSuspend(const std::string& name) {
        auto it = active_.find(name);
        if (it != active_.end()) it->second.suspendCount++;
    }

    void trackDestroy(const std::string& name) {
        auto it = active_.find(name);
        if (it != active_.end()) it->second.wasDestroyed = true;
    }

    std::vector<LeakReport> detectLeaks() {
        std::vector<LeakReport> leaks;
        std::vector<std::string> toRemove;
        for (auto& [name, report] : active_) {
            if (!report.wasDestroyed) {
                report.isLeaked = true;
                leaks.push_back(report);
            }
            if (report.wasDestroyed) toRemove.push_back(name);
        }
        for (auto& name : toRemove) active_.erase(name);
        return leaks;
    }

    size_t activeCoroutines() const { return active_.size(); }

    void forceCleanup() { active_.clear(); }

private:
    std::unordered_map<std::string, LeakReport> active_;
};
} // namespace nexus::utility::cpp26
