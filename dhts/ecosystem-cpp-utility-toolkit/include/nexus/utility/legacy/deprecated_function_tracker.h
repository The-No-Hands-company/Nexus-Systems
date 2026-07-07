#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>

namespace nexus::utility::legacy {

/**
 * @brief Track usage of deprecated functions at runtime.
 */
class DeprecatedFunctionTracker {
public:
    struct Usage {
        std::string function;
        std::string replacement;
        std::string deprecatedSince;
        std::size_t callCount = 0;
    };

    static DeprecatedFunctionTracker& instance() {
        static DeprecatedFunctionTracker inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    void registerDeprecated(const std::string& function, const std::string& replacement = "",
                            const std::string& since = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        auto& u = usages_[function];
        u.function = function;
        u.replacement = replacement;
        u.deprecatedSince = since;
    }

    /// Record a call to a deprecated function.
    void recordCall(const std::string& function) {
        std::lock_guard<std::mutex> lk(mutex_);
        auto& u = usages_[function];
        u.function = function;
        ++u.callCount;
        ++totalCalls_;
    }

    std::size_t callCount(const std::string& function) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = usages_.find(function);
        return it == usages_.end() ? 0 : it->second.callCount;
    }

    /// Deprecated functions actually being called (candidates for migration).
    std::vector<Usage> activeUsages() const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::vector<Usage> out;
        for (const auto& [f, u] : usages_) if (u.callCount > 0) out.push_back(u);
        return out;
    }

    std::size_t totalCalls() const { return totalCalls_.load(); }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        usages_.clear();
        totalCalls_ = 0;
    }

private:
    DeprecatedFunctionTracker() = default;
    ~DeprecatedFunctionTracker() = default;
    DeprecatedFunctionTracker(const DeprecatedFunctionTracker&) = delete;
    DeprecatedFunctionTracker& operator=(const DeprecatedFunctionTracker&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::unordered_map<std::string, Usage> usages_;
    std::atomic<std::size_t> totalCalls_{0};
};

} // namespace nexus::utility::legacy
