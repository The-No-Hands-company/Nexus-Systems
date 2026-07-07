#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <chrono>
#include <cstdint>

namespace nexus::utility::plugin {

/**
 * @brief Track loaded dynamic libraries (DLLs / shared objects) with timestamps.
 */
class DllLoadTracker {
public:
    struct LoadedModule {
        std::string path;
        void* handle = nullptr;
        std::uint64_t loadTimeMs = 0;
        bool loaded = true;
    };

    static DllLoadTracker& instance() {
        static DllLoadTracker inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    void recordLoad(const std::string& path, void* handle) {
        auto ts = nowMs();
        std::lock_guard<std::mutex> lk(mutex_);
        modules_[path] = {path, handle, ts, true};
        ++totalLoads_;
    }

    void recordUnload(const std::string& path) {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = modules_.find(path);
        if (it != modules_.end()) {
            it->second.loaded = false;
            it->second.handle = nullptr;
        }
    }

    bool isLoaded(const std::string& path) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = modules_.find(path);
        return it != modules_.end() && it->second.loaded;
    }

    std::vector<LoadedModule> loadedModules() const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::vector<LoadedModule> out;
        for (const auto& [p, m] : modules_) if (m.loaded) out.push_back(m);
        return out;
    }

    std::size_t currentlyLoaded() const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::size_t n = 0;
        for (const auto& [p, m] : modules_) if (m.loaded) ++n;
        return n;
    }

    std::size_t totalLoads() const { return totalLoads_.load(); }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        modules_.clear();
        totalLoads_ = 0;
    }

private:
    DllLoadTracker() = default;
    ~DllLoadTracker() = default;
    DllLoadTracker(const DllLoadTracker&) = delete;
    DllLoadTracker& operator=(const DllLoadTracker&) = delete;

    static std::uint64_t nowMs() {
        return static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
    }

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::unordered_map<std::string, LoadedModule> modules_;
    std::atomic<std::size_t> totalLoads_{0};
};

} // namespace nexus::utility::plugin
