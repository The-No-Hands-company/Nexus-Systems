#pragma once

#include <string>
#include <vector>
#include <deque>
#include <atomic>
#include <mutex>
#include <chrono>
#include <cstdint>

namespace nexus::utility::timetravel {

/**
 * @brief Circular buffer recording recent function calls for post-mortem replay.
 */
class CallHistoryBuffer {
public:
    struct CallEntry {
        std::string function;
        std::string arguments;
        std::uint64_t timestampUs = 0;
        std::size_t depth = 0;
    };

    static CallHistoryBuffer& instance() {
        static CallHistoryBuffer inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    void setCapacity(std::size_t capacity) {
        std::lock_guard<std::mutex> lk(mutex_);
        capacity_ = capacity ? capacity : 1;
        while (buffer_.size() > capacity_) buffer_.pop_front();
    }

    void recordCall(const std::string& function, const std::string& arguments = "") {
        auto ts = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count());
        std::lock_guard<std::mutex> lk(mutex_);
        buffer_.push_back({function, arguments, ts, depth_});
        if (buffer_.size() > capacity_) buffer_.pop_front();
        ++total_;
    }

    void enter() { ++depth_; }
    void leave() { if (depth_ > 0) --depth_; }

    /// Most recent `n` calls (newest last).
    std::vector<CallEntry> recent(std::size_t n) const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::vector<CallEntry> out;
        std::size_t start = buffer_.size() > n ? buffer_.size() - n : 0;
        for (std::size_t i = start; i < buffer_.size(); ++i) out.push_back(buffer_[i]);
        return out;
    }

    std::vector<CallEntry> all() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return {buffer_.begin(), buffer_.end()};
    }

    std::size_t size() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return buffer_.size();
    }
    std::size_t totalRecorded() const { return total_.load(); }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        buffer_.clear();
        depth_ = 0;
        total_ = 0;
    }

private:
    CallHistoryBuffer() = default;
    ~CallHistoryBuffer() = default;
    CallHistoryBuffer(const CallHistoryBuffer&) = delete;
    CallHistoryBuffer& operator=(const CallHistoryBuffer&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::size_t capacity_ = 1024;
    std::deque<CallEntry> buffer_;
    std::size_t depth_ = 0;
    std::atomic<std::size_t> total_{0};
};

} // namespace nexus::utility::timetravel
