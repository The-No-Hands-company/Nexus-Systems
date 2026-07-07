#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <atomic>
#include <mutex>
#include <cstdint>

namespace nexus::utility::timetravel {

/**
 * @brief Snapshot named data blobs and compare them across time.
 */
class DataSnapshotManager {
public:
    struct Snapshot {
        std::string key;
        std::vector<std::uint8_t> data;
        std::uint64_t version = 0;
    };

    struct Diff {
        bool changed = false;
        std::size_t oldSize = 0;
        std::size_t newSize = 0;
        std::size_t changedBytes = 0;
    };

    static DataSnapshotManager& instance() {
        static DataSnapshotManager inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    /// Take a snapshot; keeps previous version for comparison.
    std::uint64_t snapshot(const std::string& key, const std::vector<std::uint8_t>& data) {
        std::lock_guard<std::mutex> lk(mutex_);
        auto& hist = history_[key];
        std::uint64_t version = hist.size();
        hist.push_back({key, data, version});
        return version;
    }

    /// Diff the two most recent snapshots for a key.
    Diff diffLatest(const std::string& key) const {
        std::lock_guard<std::mutex> lk(mutex_);
        Diff d;
        auto it = history_.find(key);
        if (it == history_.end() || it->second.size() < 2) return d;
        const auto& a = it->second[it->second.size() - 2].data;
        const auto& b = it->second.back().data;
        d.oldSize = a.size();
        d.newSize = b.size();
        std::size_t n = std::min(a.size(), b.size());
        for (std::size_t i = 0; i < n; ++i) if (a[i] != b[i]) ++d.changedBytes;
        d.changedBytes += (a.size() > b.size() ? a.size() - b.size() : b.size() - a.size());
        d.changed = d.changedBytes > 0;
        return d;
    }

    bool getVersion(const std::string& key, std::uint64_t version,
                    std::vector<std::uint8_t>& out) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = history_.find(key);
        if (it == history_.end() || version >= it->second.size()) return false;
        out = it->second[version].data;
        return true;
    }

    std::size_t versionCount(const std::string& key) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = history_.find(key);
        return it == history_.end() ? 0 : it->second.size();
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        history_.clear();
    }

private:
    DataSnapshotManager() = default;
    ~DataSnapshotManager() = default;
    DataSnapshotManager(const DataSnapshotManager&) = delete;
    DataSnapshotManager& operator=(const DataSnapshotManager&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::unordered_map<std::string, std::vector<Snapshot>> history_;
};

} // namespace nexus::utility::timetravel
