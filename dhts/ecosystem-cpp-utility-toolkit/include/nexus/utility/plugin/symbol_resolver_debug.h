#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>

namespace nexus::utility::plugin {

/**
 * @brief Track symbol lookups and misses during plugin symbol resolution.
 */
class SymbolResolverDebug {
public:
    struct SymbolStats {
        std::string symbol;
        std::size_t lookups = 0;
        std::size_t hits = 0;
        std::size_t misses = 0;
        void* address = nullptr;
    };

    static SymbolResolverDebug& instance() {
        static SymbolResolverDebug inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    /// Record a symbol lookup and whether it resolved.
    void recordLookup(const std::string& symbol, void* address) {
        std::lock_guard<std::mutex> lk(mutex_);
        auto& s = symbols_[symbol];
        s.symbol = symbol;
        ++s.lookups;
        if (address) { ++s.hits; s.address = address; ++totalHits_; }
        else { ++s.misses; ++totalMisses_; }
    }

    SymbolStats stats(const std::string& symbol) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = symbols_.find(symbol);
        return it == symbols_.end() ? SymbolStats{symbol, 0, 0, 0, nullptr} : it->second;
    }

    std::vector<SymbolStats> unresolvedSymbols() const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::vector<SymbolStats> out;
        for (const auto& [n, s] : symbols_)
            if (s.hits == 0 && s.misses > 0) out.push_back(s);
        return out;
    }

    std::size_t totalHits() const { return totalHits_.load(); }
    std::size_t totalMisses() const { return totalMisses_.load(); }

    double hitRate() const {
        std::size_t h = totalHits_.load(), m = totalMisses_.load();
        return (h + m) == 0 ? 0.0 : static_cast<double>(h) / (h + m);
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        symbols_.clear();
        totalHits_ = 0;
        totalMisses_ = 0;
    }

private:
    SymbolResolverDebug() = default;
    ~SymbolResolverDebug() = default;
    SymbolResolverDebug(const SymbolResolverDebug&) = delete;
    SymbolResolverDebug& operator=(const SymbolResolverDebug&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::unordered_map<std::string, SymbolStats> symbols_;
    std::atomic<std::size_t> totalHits_{0};
    std::atomic<std::size_t> totalMisses_{0};
};

} // namespace nexus::utility::plugin
