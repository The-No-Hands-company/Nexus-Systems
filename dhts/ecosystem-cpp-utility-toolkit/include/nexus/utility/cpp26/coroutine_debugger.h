#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <source_location>

namespace nexus::utility::cpp26 {

/// Debug C++26 coroutines (extended promise_type, as_tuple, sync_wait refinements)
class CoroutineDebugger {
public:
    struct CoroutineState {
        std::string name;
        bool suspended = false;
        bool done = false;
        size_t suspendCount = 0;
        size_t resumeCount = 0;
        std::chrono::microseconds totalSuspendedTime{0};
        std::chrono::steady_clock::time_point lastSuspend;
        std::source_location creationSite;
    };

    size_t registerCoroutine(const std::string& name,
                             std::source_location loc = std::source_location::current()) {
        size_t id = nextId_++;
        states_[id] = {name, false, false, 0, 0, std::chrono::microseconds(0),
                       std::chrono::steady_clock::now(), loc};
        return id;
    }

    void recordSuspend(size_t id) {
        auto it = states_.find(id);
        if (it == states_.end()) return;
        it->second.suspended = true;
        it->second.suspendCount++;
        it->second.lastSuspend = std::chrono::steady_clock::now();
    }

    void recordResume(size_t id) {
        auto it = states_.find(id);
        if (it == states_.end()) return;
        it->second.suspended = false;
        it->second.resumeCount++;
        auto now = std::chrono::steady_clock::now();
        it->second.totalSuspendedTime +=
            std::chrono::duration_cast<std::chrono::microseconds>(now - it->second.lastSuspend);
    }

    void recordDone(size_t id) {
        auto it = states_.find(id);
        if (it != states_.end()) it->second.done = true;
    }

    CoroutineState getState(size_t id) const {
        auto it = states_.find(id);
        return it != states_.end() ? it->second : CoroutineState{};
    }

    std::vector<CoroutineState> getAllStates() const {
        std::vector<CoroutineState> r;
        for (auto& [id, s] : states_) r.push_back(s);
        return r;
    }

    size_t activeCount() const {
        size_t c = 0;
        for (auto& [id, s] : states_) if (!s.done) c++;
        return c;
    }

    void clear() { states_.clear(); nextId_ = 0; }

private:
    size_t nextId_ = 0;
    std::unordered_map<size_t, CoroutineState> states_;
};
} // namespace nexus::utility::cpp26
