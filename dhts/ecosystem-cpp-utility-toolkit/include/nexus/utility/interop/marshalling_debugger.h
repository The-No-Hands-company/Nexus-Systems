#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>

namespace nexus::utility::interop {

/**
 * @brief Track data marshalling operations across boundaries.
 */
class MarshallingDebugger {
public:
    enum class Direction { ToNative, FromNative };

    struct MarshalRecord {
        std::string typeName;
        Direction direction;
        std::size_t bytes = 0;
        bool success = true;
    };

    struct TypeStats {
        std::string typeName;
        std::size_t operations = 0;
        std::size_t totalBytes = 0;
        std::size_t failures = 0;
    };

    static MarshallingDebugger& instance() {
        static MarshallingDebugger inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    void recordMarshal(const std::string& typeName, Direction dir, std::size_t bytes,
                       bool success = true) {
        std::lock_guard<std::mutex> lk(mutex_);
        history_.push_back({typeName, dir, bytes, success});
        auto& s = stats_[typeName];
        s.typeName = typeName;
        ++s.operations;
        s.totalBytes += bytes;
        if (!success) ++s.failures;
        ++total_;
        totalBytes_ += bytes;
    }

    TypeStats stats(const std::string& typeName) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = stats_.find(typeName);
        return it == stats_.end() ? TypeStats{typeName, 0, 0, 0} : it->second;
    }

    std::size_t totalOperations() const { return total_.load(); }
    std::size_t totalBytes() const { return totalBytes_.load(); }

    std::vector<MarshalRecord> history() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return history_;
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        history_.clear();
        stats_.clear();
        total_ = 0;
        totalBytes_ = 0;
    }

private:
    MarshallingDebugger() = default;
    ~MarshallingDebugger() = default;
    MarshallingDebugger(const MarshallingDebugger&) = delete;
    MarshallingDebugger& operator=(const MarshallingDebugger&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::vector<MarshalRecord> history_;
    std::unordered_map<std::string, TypeStats> stats_;
    std::atomic<std::size_t> total_{0};
    std::atomic<std::size_t> totalBytes_{0};
};

} // namespace nexus::utility::interop
