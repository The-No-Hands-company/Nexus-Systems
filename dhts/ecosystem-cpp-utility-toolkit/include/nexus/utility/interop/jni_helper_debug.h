#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>

namespace nexus::utility::interop {

/**
 * @brief Track JNI (Java Native Interface) calls and local/global reference usage.
 */
class JniHelperDebug {
public:
    struct MethodStats {
        std::string method;
        std::size_t calls = 0;
        std::size_t exceptions = 0;
    };

    static JniHelperDebug& instance() {
        static JniHelperDebug inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    void recordCall(const std::string& method, bool threwException = false) {
        std::lock_guard<std::mutex> lk(mutex_);
        auto& s = methods_[method];
        s.method = method;
        ++s.calls;
        if (threwException) ++s.exceptions;
        ++totalCalls_;
    }

    void createLocalRef() { ++localRefs_; }
    void deleteLocalRef() { if (localRefs_ > 0) --localRefs_; }
    void createGlobalRef() { ++globalRefs_; }
    void deleteGlobalRef() { if (globalRefs_ > 0) --globalRefs_; }

    /// Local reference table overflow is a common JNI bug (default table ~512).
    bool localRefLeakSuspected(std::size_t threshold = 512) const {
        return localRefs_.load() > threshold;
    }

    std::size_t liveLocalRefs() const { return localRefs_.load(); }
    std::size_t liveGlobalRefs() const { return globalRefs_.load(); }
    std::size_t totalCalls() const { return totalCalls_.load(); }

    MethodStats stats(const std::string& method) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = methods_.find(method);
        return it == methods_.end() ? MethodStats{method, 0, 0} : it->second;
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        methods_.clear();
        totalCalls_ = 0;
        localRefs_ = 0;
        globalRefs_ = 0;
    }

private:
    JniHelperDebug() = default;
    ~JniHelperDebug() = default;
    JniHelperDebug(const JniHelperDebug&) = delete;
    JniHelperDebug& operator=(const JniHelperDebug&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::unordered_map<std::string, MethodStats> methods_;
    std::atomic<std::size_t> totalCalls_{0};
    std::atomic<std::size_t> localRefs_{0};
    std::atomic<std::size_t> globalRefs_{0};
};

} // namespace nexus::utility::interop
