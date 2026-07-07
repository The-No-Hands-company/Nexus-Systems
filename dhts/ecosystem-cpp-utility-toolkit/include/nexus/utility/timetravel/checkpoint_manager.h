#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <cstdint>

namespace nexus::utility::timetravel {

/**
 * @brief Save and restore named checkpoints of arbitrary serialized state.
 */
class CheckpointManager {
public:
    struct Checkpoint {
        std::string name;
        std::vector<std::uint8_t> data;
        std::uint64_t sequence = 0;
    };

    static CheckpointManager& instance() {
        static CheckpointManager inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    /// Save a named checkpoint (overwrites an existing one with the same name).
    std::uint64_t save(const std::string& name, const std::vector<std::uint8_t>& data) {
        std::lock_guard<std::mutex> lk(mutex_);
        std::uint64_t seq = ++sequence_;
        checkpoints_[name] = {name, data, seq};
        return seq;
    }

    /// Restore a checkpoint's data; returns false if it doesn't exist.
    bool restore(const std::string& name, std::vector<std::uint8_t>& out) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = checkpoints_.find(name);
        if (it == checkpoints_.end()) return false;
        out = it->second.data;
        return true;
    }

    bool exists(const std::string& name) const {
        std::lock_guard<std::mutex> lk(mutex_);
        return checkpoints_.count(name) > 0;
    }

    bool remove(const std::string& name) {
        std::lock_guard<std::mutex> lk(mutex_);
        return checkpoints_.erase(name) > 0;
    }

    std::vector<std::string> names() const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::vector<std::string> out;
        for (const auto& [n, c] : checkpoints_) out.push_back(n);
        return out;
    }

    std::size_t count() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return checkpoints_.size();
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        checkpoints_.clear();
        sequence_ = 0;
    }

private:
    CheckpointManager() = default;
    ~CheckpointManager() = default;
    CheckpointManager(const CheckpointManager&) = delete;
    CheckpointManager& operator=(const CheckpointManager&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::unordered_map<std::string, Checkpoint> checkpoints_;
    std::uint64_t sequence_ = 0;
};

} // namespace nexus::utility::timetravel
