#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>

namespace nexus::utility::legacy {

/**
 * @brief Track platform-specific workarounds and their applicability.
 */
class PlatformCompatibilityLayer {
public:
    enum class Platform { Windows, Linux, MacOS, BSD, Android, IOS, Any };

    struct Workaround {
        std::string id;
        std::string description;
        Platform platform = Platform::Any;
        std::string reason;
        std::size_t activations = 0;
        bool obsolete = false;
    };

    static PlatformCompatibilityLayer& instance() {
        static PlatformCompatibilityLayer inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        current_ = detectPlatform();
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    static Platform detectPlatform() {
#if defined(_WIN32)
        return Platform::Windows;
#elif defined(__APPLE__)
        return Platform::MacOS;
#elif defined(__ANDROID__)
        return Platform::Android;
#elif defined(__linux__)
        return Platform::Linux;
#elif defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
        return Platform::BSD;
#else
        return Platform::Any;
#endif
    }

    Platform currentPlatform() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return current_;
    }

    void registerWorkaround(const std::string& id, const std::string& description,
                            Platform platform, const std::string& reason = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        workarounds_[id] = {id, description, platform, reason, 0, false};
    }

    bool appliesToCurrent(const std::string& id) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = workarounds_.find(id);
        if (it == workarounds_.end()) return false;
        return it->second.platform == Platform::Any || it->second.platform == current_;
    }

    /// Record that a workaround was activated; returns whether it applied.
    bool activate(const std::string& id) {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = workarounds_.find(id);
        if (it == workarounds_.end()) return false;
        bool applies = it->second.platform == Platform::Any || it->second.platform == current_;
        if (applies) { ++it->second.activations; ++totalActivations_; }
        return applies;
    }

    void markObsolete(const std::string& id) {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = workarounds_.find(id);
        if (it != workarounds_.end()) it->second.obsolete = true;
    }

    /// Obsolete workarounds still being activated (should be removed).
    std::vector<Workaround> staleWorkarounds() const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::vector<Workaround> out;
        for (const auto& [id, w] : workarounds_)
            if (w.obsolete && w.activations > 0) out.push_back(w);
        return out;
    }

    std::size_t totalActivations() const { return totalActivations_.load(); }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        workarounds_.clear();
        totalActivations_ = 0;
    }

private:
    PlatformCompatibilityLayer() : current_(detectPlatform()) {}
    ~PlatformCompatibilityLayer() = default;
    PlatformCompatibilityLayer(const PlatformCompatibilityLayer&) = delete;
    PlatformCompatibilityLayer& operator=(const PlatformCompatibilityLayer&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    Platform current_;
    std::unordered_map<std::string, Workaround> workarounds_;
    std::atomic<std::size_t> totalActivations_{0};
};

} // namespace nexus::utility::legacy
