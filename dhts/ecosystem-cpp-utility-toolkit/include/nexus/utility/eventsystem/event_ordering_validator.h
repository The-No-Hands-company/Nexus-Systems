#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>

namespace nexus::utility::eventsystem {

/// @brief Validates that per-stream event sequence numbers are monotonically increasing.
class EventOrderingValidator {
public:
    static EventOrderingValidator& instance() {
        static EventOrderingValidator inst;
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
        streams_.clear();
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    /// @brief Record an event and update ordering state; counts out-of-order arrivals.
    /// @return true if this sequence maintained monotonic ordering.
    bool recordEvent(const std::string& stream, uint64_t sequence) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto& s = streams_[stream];
        bool in_order = !s.seen || sequence > s.last_sequence;
        if (s.seen && !in_order) {
            ++s.out_of_order;
        }
        s.last_sequence = sequence;
        s.seen = true;
        return in_order;
    }

    /// @brief Non-mutating check whether a sequence would be in order for a stream.
    bool checkOrdering(const std::string& stream, uint64_t sequence) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = streams_.find(stream);
        if (it == streams_.end() || !it->second.seen) return true;
        return sequence > it->second.last_sequence;
    }

    size_t getOutOfOrderCount(const std::string& stream) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = streams_.find(stream);
        return it != streams_.end() ? it->second.out_of_order : 0;
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        streams_.clear();
    }

private:
    struct StreamState {
        uint64_t last_sequence = 0;
        size_t out_of_order = 0;
        bool seen = false;
    };

    EventOrderingValidator() = default;
    ~EventOrderingValidator() = default;
    EventOrderingValidator(const EventOrderingValidator&) = delete;
    EventOrderingValidator& operator=(const EventOrderingValidator&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::map<std::string, StreamState> streams_;
};

} // namespace nexus::utility::eventsystem
