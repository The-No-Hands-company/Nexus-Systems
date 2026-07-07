#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <cstddef>
#include <mutex>

namespace nexus::utility::string {

/// @brief Track a string interning pool backed by a vector.
class StringInterningTracker {
public:
    static constexpr size_t npos = static_cast<size_t>(-1);

    static StringInterningTracker& instance() {
        static StringInterningTracker inst;
        return inst;
    }

    void initialize() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = true;
        pool_.clear();
        index_.clear();
        total_bytes_ = 0;
    }

    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
        pool_.clear();
        index_.clear();
        total_bytes_ = 0;
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    /// Intern a string, returning its stable index. Duplicates share an index.
    size_t intern(const std::string& str) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = index_.find(str);
        if (it != index_.end()) return it->second;
        size_t idx = pool_.size();
        pool_.push_back(str);
        index_.emplace(str, idx);
        total_bytes_ += str.size();
        return idx;
    }

    /// Look up an interned string by index ("" if out of range).
    std::string lookup(size_t index) const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (index >= pool_.size()) return {};
        return pool_[index];
    }

    /// Number of unique interned strings.
    size_t getInternCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return pool_.size();
    }

    /// Total bytes of unique interned strings.
    size_t getTotalBytes() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return total_bytes_;
    }

private:
    StringInterningTracker() = default;
    ~StringInterningTracker() = default;
    StringInterningTracker(const StringInterningTracker&) = delete;
    StringInterningTracker& operator=(const StringInterningTracker&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::vector<std::string> pool_;
    std::unordered_map<std::string, size_t> index_;
    size_t total_bytes_ = 0;
};

} // namespace nexus::utility::string
