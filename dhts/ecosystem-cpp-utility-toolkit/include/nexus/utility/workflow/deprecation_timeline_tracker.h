#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <cstdint>

namespace nexus::utility::workflow {

/**
 * @brief Track deprecation schedules and their timelines.
 */
class DeprecationTimelineTracker {
public:
    enum class Phase { Active, Deprecated, Sunset, Removed };

    struct Entry {
        std::string symbol;
        std::string deprecatedInVersion;
        std::string removeInVersion;
        std::uint64_t deprecatedOnDate = 0;   // days since epoch
        std::uint64_t removeOnDate = 0;
        Phase phase = Phase::Deprecated;
        std::string replacement;
    };

    static DeprecationTimelineTracker& instance() {
        static DeprecationTimelineTracker inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    void addDeprecation(const std::string& symbol, const std::string& deprecatedIn,
                        const std::string& removeIn, const std::string& replacement = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        entries_[symbol] = {symbol, deprecatedIn, removeIn, 0, 0, Phase::Deprecated, replacement};
    }

    void setDates(const std::string& symbol, std::uint64_t deprecatedOn, std::uint64_t removeOn) {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = entries_.find(symbol);
        if (it != entries_.end()) {
            it->second.deprecatedOnDate = deprecatedOn;
            it->second.removeOnDate = removeOn;
        }
    }

    void setPhase(const std::string& symbol, Phase phase) {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = entries_.find(symbol);
        if (it != entries_.end()) it->second.phase = phase;
    }

    /// Entries whose removal date is on or before `currentDate`.
    std::vector<Entry> dueForRemoval(std::uint64_t currentDate) const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::vector<Entry> out;
        for (const auto& [s, e] : entries_)
            if (e.removeOnDate != 0 && e.removeOnDate <= currentDate &&
                e.phase != Phase::Removed)
                out.push_back(e);
        return out;
    }

    Entry get(const std::string& symbol) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = entries_.find(symbol);
        return it == entries_.end() ? Entry{symbol, "", "", 0, 0, Phase::Active, ""} : it->second;
    }

    std::size_t count() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return entries_.size();
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        entries_.clear();
    }

private:
    DeprecationTimelineTracker() = default;
    ~DeprecationTimelineTracker() = default;
    DeprecationTimelineTracker(const DeprecationTimelineTracker&) = delete;
    DeprecationTimelineTracker& operator=(const DeprecationTimelineTracker&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::unordered_map<std::string, Entry> entries_;
};

} // namespace nexus::utility::workflow
