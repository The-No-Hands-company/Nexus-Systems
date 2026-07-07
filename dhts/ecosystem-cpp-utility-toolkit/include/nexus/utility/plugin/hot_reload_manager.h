#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <atomic>
#include <mutex>
#include <chrono>
#include <cstdint>

namespace nexus::utility::plugin {

/**
 * @brief Track file changes and trigger hot-reloads of plugins.
 */
class HotReloadManager {
public:
    using ReloadCallback = std::function<void(const std::string& path)>;

    struct WatchedFile {
        std::string path;
        std::uint64_t lastModified = 0;
        std::size_t reloadCount = 0;
    };

    static HotReloadManager& instance() {
        static HotReloadManager inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    void watch(const std::string& path, std::uint64_t currentMtime) {
        std::lock_guard<std::mutex> lk(mutex_);
        watched_[path] = {path, currentMtime, 0};
    }

    void onReload(ReloadCallback cb) {
        std::lock_guard<std::mutex> lk(mutex_);
        callback_ = std::move(cb);
    }

    /// Report a file's current mtime; triggers a reload if it changed.
    bool notifyModified(const std::string& path, std::uint64_t mtime) {
        ReloadCallback cb;
        {
            std::lock_guard<std::mutex> lk(mutex_);
            auto it = watched_.find(path);
            if (it == watched_.end()) { watched_[path] = {path, mtime, 0}; return false; }
            if (mtime <= it->second.lastModified) return false;
            it->second.lastModified = mtime;
            ++it->second.reloadCount;
            ++totalReloads_;
            cb = callback_;
        }
        if (cb) cb(path);
        return true;
    }

    std::size_t reloadCount(const std::string& path) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = watched_.find(path);
        return it == watched_.end() ? 0 : it->second.reloadCount;
    }

    std::size_t totalReloads() const { return totalReloads_.load(); }

    std::vector<WatchedFile> watchedFiles() const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::vector<WatchedFile> out;
        for (const auto& [p, w] : watched_) out.push_back(w);
        return out;
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        watched_.clear();
        totalReloads_ = 0;
    }

private:
    HotReloadManager() = default;
    ~HotReloadManager() = default;
    HotReloadManager(const HotReloadManager&) = delete;
    HotReloadManager& operator=(const HotReloadManager&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::unordered_map<std::string, WatchedFile> watched_;
    ReloadCallback callback_;
    std::atomic<std::size_t> totalReloads_{0};
};

} // namespace nexus::utility::plugin
