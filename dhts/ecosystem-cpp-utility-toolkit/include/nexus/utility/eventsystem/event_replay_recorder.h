#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace nexus::utility::eventsystem {

/// @brief Records events with monotonic sequence numbers for later replay.
class EventReplayRecorder {
public:
    struct Event {
        uint64_t sequence = 0;
        std::string type;
        std::string data;
        uint64_t timestamp_ms = 0;
    };

    static EventReplayRecorder& instance() {
        static EventReplayRecorder inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = true;
        config_ = config;
    }

    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
        events_.clear();
        next_sequence_ = 0;
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    void recordEvent(const std::string& type, const std::string& data) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count();
        events_.push_back({next_sequence_++, type, data, static_cast<uint64_t>(now)});
    }

    std::vector<Event> getEvents() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return events_;
    }

    std::vector<Event> getEventsByType(const std::string& type) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<Event> out;
        for (const auto& e : events_) {
            if (e.type == type) out.push_back(e);
        }
        return out;
    }

    /// @brief Return events with a sequence number strictly greater than the given one.
    std::vector<Event> getEventsSince(uint64_t sequence) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<Event> out;
        for (const auto& e : events_) {
            if (e.sequence > sequence) out.push_back(e);
        }
        return out;
    }

    size_t getEventCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return events_.size();
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        events_.clear();
        next_sequence_ = 0;
    }

    void reset() { clear(); }

private:
    EventReplayRecorder() = default;
    ~EventReplayRecorder() = default;
    EventReplayRecorder(const EventReplayRecorder&) = delete;
    EventReplayRecorder& operator=(const EventReplayRecorder&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::vector<Event> events_;
    uint64_t next_sequence_ = 0;
};

} // namespace nexus::utility::eventsystem
