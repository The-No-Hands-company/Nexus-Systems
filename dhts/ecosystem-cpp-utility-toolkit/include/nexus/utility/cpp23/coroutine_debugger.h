#pragma once

#include <string>
#include <unordered_map>
#include <mutex>

namespace nexus::utility::cpp23 {

/// @brief Track lifecycle and state transitions of coroutines.
class CoroutineDebugger {
public:
    enum class State { Created, Suspended, Running, Destroyed };

    static CoroutineDebugger& instance() {
        static CoroutineDebugger inst;
        return inst;
    }

    void initialize() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = true;
    }

    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
        coroutines_.clear();
        active_ = 0;
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    void recordCoroutineCreate(const std::string& name, void* handle) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto& info = coroutines_[handle];
        info.name = name;
        info.state = State::Created;
        ++active_;
    }

    void recordCoroutineDestroy(void* handle) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = coroutines_.find(handle);
        if (it != coroutines_.end()) {
            if (it->second.state != State::Destroyed && active_ > 0) --active_;
            it->second.state = State::Destroyed;
        }
    }

    void recordSuspend(void* handle) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = coroutines_.find(handle);
        if (it != coroutines_.end()) it->second.state = State::Suspended;
    }

    void recordResume(void* handle) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = coroutines_.find(handle);
        if (it != coroutines_.end()) it->second.state = State::Running;
    }

    /// Number of coroutines created but not yet destroyed.
    size_t getActiveCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return active_;
    }

    State getState(void* handle) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = coroutines_.find(handle);
        return it != coroutines_.end() ? it->second.state : State::Destroyed;
    }

private:
    struct Info {
        std::string name;
        State state = State::Created;
    };

    CoroutineDebugger() = default;
    ~CoroutineDebugger() = default;
    CoroutineDebugger(const CoroutineDebugger&) = delete;
    CoroutineDebugger& operator=(const CoroutineDebugger&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    size_t active_ = 0;
    std::unordered_map<void*, Info> coroutines_;
};

} // namespace nexus::utility::cpp23
