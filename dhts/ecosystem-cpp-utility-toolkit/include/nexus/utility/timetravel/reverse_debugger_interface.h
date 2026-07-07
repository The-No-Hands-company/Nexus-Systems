#pragma once

#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <cstdint>

namespace nexus::utility::timetravel {

/**
 * @brief Interface for stepping backward through recorded execution states.
 */
class ReverseDebuggerInterface {
public:
    struct ExecutionState {
        std::uint64_t step = 0;
        std::string location;
        std::vector<std::uint8_t> snapshot;
    };

    static ReverseDebuggerInterface& instance() {
        static ReverseDebuggerInterface inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    /// Record a new execution state (moves the cursor to the end).
    void recordStep(const std::string& location, const std::vector<std::uint8_t>& snapshot = {}) {
        std::lock_guard<std::mutex> lk(mutex_);
        states_.push_back({states_.size(), location, snapshot});
        cursor_ = states_.size() - 1;
    }

    /// Step backward; returns false if already at the beginning.
    bool stepBack() {
        std::lock_guard<std::mutex> lk(mutex_);
        if (states_.empty() || cursor_ == 0) return false;
        --cursor_;
        return true;
    }

    /// Step forward; returns false if already at the latest state.
    bool stepForward() {
        std::lock_guard<std::mutex> lk(mutex_);
        if (states_.empty() || cursor_ + 1 >= states_.size()) return false;
        ++cursor_;
        return true;
    }

    /// Jump to a specific step index.
    bool seek(std::uint64_t step) {
        std::lock_guard<std::mutex> lk(mutex_);
        if (step >= states_.size()) return false;
        cursor_ = step;
        return true;
    }

    ExecutionState current() const {
        std::lock_guard<std::mutex> lk(mutex_);
        if (states_.empty()) return {};
        return states_[cursor_];
    }

    std::uint64_t position() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return cursor_;
    }
    std::size_t stepCount() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return states_.size();
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        states_.clear();
        cursor_ = 0;
    }

private:
    ReverseDebuggerInterface() = default;
    ~ReverseDebuggerInterface() = default;
    ReverseDebuggerInterface(const ReverseDebuggerInterface&) = delete;
    ReverseDebuggerInterface& operator=(const ReverseDebuggerInterface&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::vector<ExecutionState> states_;
    std::uint64_t cursor_ = 0;
};

} // namespace nexus::utility::timetravel
