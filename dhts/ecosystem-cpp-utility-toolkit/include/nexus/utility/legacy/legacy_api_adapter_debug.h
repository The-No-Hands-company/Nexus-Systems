#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>

namespace nexus::utility::legacy {

/**
 * @brief Track calls routed through legacy API adapters (shims).
 */
class LegacyApiAdapterDebug {
public:
    struct AdapterStats {
        std::string legacyApi;
        std::string modernApi;
        std::size_t calls = 0;
        std::size_t translationErrors = 0;
    };

    static LegacyApiAdapterDebug& instance() {
        static LegacyApiAdapterDebug inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    void registerAdapter(const std::string& legacyApi, const std::string& modernApi) {
        std::lock_guard<std::mutex> lk(mutex_);
        auto& a = adapters_[legacyApi];
        a.legacyApi = legacyApi;
        a.modernApi = modernApi;
    }

    void recordCall(const std::string& legacyApi, bool translationError = false) {
        std::lock_guard<std::mutex> lk(mutex_);
        auto& a = adapters_[legacyApi];
        a.legacyApi = legacyApi;
        ++a.calls;
        if (translationError) ++a.translationErrors;
        ++totalCalls_;
    }

    AdapterStats stats(const std::string& legacyApi) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = adapters_.find(legacyApi);
        return it == adapters_.end() ? AdapterStats{legacyApi, "", 0, 0} : it->second;
    }

    std::vector<AdapterStats> allStats() const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::vector<AdapterStats> out;
        for (const auto& [k, a] : adapters_) out.push_back(a);
        return out;
    }

    std::size_t totalCalls() const { return totalCalls_.load(); }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        adapters_.clear();
        totalCalls_ = 0;
    }

private:
    LegacyApiAdapterDebug() = default;
    ~LegacyApiAdapterDebug() = default;
    LegacyApiAdapterDebug(const LegacyApiAdapterDebug&) = delete;
    LegacyApiAdapterDebug& operator=(const LegacyApiAdapterDebug&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::unordered_map<std::string, AdapterStats> adapters_;
    std::atomic<std::size_t> totalCalls_{0};
};

} // namespace nexus::utility::legacy
