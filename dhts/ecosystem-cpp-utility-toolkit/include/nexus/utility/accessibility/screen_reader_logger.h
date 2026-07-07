#pragma once

#include <string>
#include <vector>
#include <deque>
#include <atomic>
#include <mutex>
#include <chrono>
#include <cstdint>

namespace nexus::utility::accessibility {

/**
 * @brief Log screen-reader announcements (aria-live regions and events).
 */
class ScreenReaderLogger {
public:
    enum class Politeness { Off, Polite, Assertive };

    struct Announcement {
        std::string message;
        Politeness politeness = Politeness::Polite;
        std::uint64_t timestampMs = 0;
    };

    static ScreenReaderLogger& instance() {
        static ScreenReaderLogger inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    void setHistoryLimit(std::size_t limit) {
        std::lock_guard<std::mutex> lk(mutex_);
        limit_ = limit;
    }

    void announce(const std::string& message, Politeness politeness = Politeness::Polite) {
        auto ts = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
        std::lock_guard<std::mutex> lk(mutex_);
        announcements_.push_back({message, politeness, ts});
        while (announcements_.size() > limit_) announcements_.pop_front();
        ++total_;
        if (politeness == Politeness::Assertive) ++assertive_;
    }

    std::vector<Announcement> history() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return {announcements_.begin(), announcements_.end()};
    }

    Announcement last() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return announcements_.empty() ? Announcement{} : announcements_.back();
    }

    std::size_t total() const { return total_.load(); }
    std::size_t assertiveCount() const { return assertive_.load(); }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        announcements_.clear();
        total_ = 0;
        assertive_ = 0;
    }

private:
    ScreenReaderLogger() = default;
    ~ScreenReaderLogger() = default;
    ScreenReaderLogger(const ScreenReaderLogger&) = delete;
    ScreenReaderLogger& operator=(const ScreenReaderLogger&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::size_t limit_ = 256;
    std::deque<Announcement> announcements_;
    std::atomic<std::size_t> total_{0};
    std::atomic<std::size_t> assertive_{0};
};

} // namespace nexus::utility::accessibility
