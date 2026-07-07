#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <atomic>

namespace nexus::utility::codequality {

/**
 * @brief Detect potentially dead code by tracking defined vs. used symbols.
 */
class DeadCodeDetector {
public:
    struct SymbolInfo {
        std::string symbol;
        std::string location;
        std::size_t useCount = 0;
    };

    static DeadCodeDetector& instance() {
        static DeadCodeDetector inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    void markDefined(const std::string& symbol, const std::string& location = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        auto& s = defined_[symbol];
        s.symbol = symbol;
        if (!location.empty()) s.location = location;
    }

    void markUsed(const std::string& symbol) {
        std::lock_guard<std::mutex> lk(mutex_);
        ++defined_[symbol].useCount;
        defined_[symbol].symbol = symbol;
    }

    bool isDead(const std::string& symbol) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = defined_.find(symbol);
        return it != defined_.end() && it->second.useCount == 0;
    }

    std::vector<SymbolInfo> deadSymbols() const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::vector<SymbolInfo> out;
        for (const auto& [sym, info] : defined_)
            if (info.useCount == 0) out.push_back(info);
        return out;
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        defined_.clear();
    }

private:
    DeadCodeDetector() = default;
    ~DeadCodeDetector() = default;
    DeadCodeDetector(const DeadCodeDetector&) = delete;
    DeadCodeDetector& operator=(const DeadCodeDetector&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::unordered_map<std::string, SymbolInfo> defined_;
};

} // namespace nexus::utility::codequality
