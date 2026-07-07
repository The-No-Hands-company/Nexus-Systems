#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>

namespace nexus::utility::cpp23 {

/// @brief Detect coroutine handles that are created but never cleaned up.
class CoroutineLeakDetector {
public:
    static CoroutineLeakDetector& instance() {
        static CoroutineLeakDetector inst;
        return inst;
    }

    void initialize() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = true;
    }

    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
        active_.clear();
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    void registerCoroutine(void* handle, const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        active_[handle] = name;
    }

    void unregisterCoroutine(void* handle) {
        std::lock_guard<std::mutex> lock(mutex_);
        active_.erase(handle);
    }

    /// Return handles that were registered but never unregistered (leaked).
    std::vector<void*> detectLeaks() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<void*> leaks;
        leaks.reserve(active_.size());
        for (const auto& [handle, name] : active_) {
            leaks.push_back(handle);
        }
        return leaks;
    }

    size_t getActiveCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return active_.size();
    }

private:
    CoroutineLeakDetector() = default;
    ~CoroutineLeakDetector() = default;
    CoroutineLeakDetector(const CoroutineLeakDetector&) = delete;
    CoroutineLeakDetector& operator=(const CoroutineLeakDetector&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::unordered_map<void*, std::string> active_;
};

} // namespace nexus::utility::cpp23
