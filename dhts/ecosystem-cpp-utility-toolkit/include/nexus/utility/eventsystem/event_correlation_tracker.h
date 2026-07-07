#pragma once

#include <cstddef>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace nexus::utility::eventsystem {

/// @brief Correlates related events by a shared correlation id.
class EventCorrelationTracker {
public:
    static EventCorrelationTracker& instance() {
        static EventCorrelationTracker inst;
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
        event_to_correlation_.clear();
        correlation_to_events_.clear();
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    void setCorrelationId(const std::string& event_id, const std::string& correlation_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto existing = event_to_correlation_.find(event_id);
        if (existing != event_to_correlation_.end()) {
            if (existing->second == correlation_id) return;
            auto& old_chain = correlation_to_events_[existing->second];
            for (auto it = old_chain.begin(); it != old_chain.end(); ++it) {
                if (*it == event_id) { old_chain.erase(it); break; }
            }
        }
        event_to_correlation_[event_id] = correlation_id;
        correlation_to_events_[correlation_id].push_back(event_id);
    }

    std::vector<std::string> getEventsByCorrelation(const std::string& correlation_id) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = correlation_to_events_.find(correlation_id);
        return it != correlation_to_events_.end() ? it->second : std::vector<std::string>{};
    }

    /// @brief Follow the correlation chain that the given event belongs to.
    std::vector<std::string> getCorrelationChain(const std::string& event_id) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = event_to_correlation_.find(event_id);
        if (it == event_to_correlation_.end()) return {};
        auto chain = correlation_to_events_.find(it->second);
        return chain != correlation_to_events_.end() ? chain->second : std::vector<std::string>{};
    }

    size_t getCorrelationCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return correlation_to_events_.size();
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        event_to_correlation_.clear();
        correlation_to_events_.clear();
    }

private:
    EventCorrelationTracker() = default;
    ~EventCorrelationTracker() = default;
    EventCorrelationTracker(const EventCorrelationTracker&) = delete;
    EventCorrelationTracker& operator=(const EventCorrelationTracker&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::map<std::string, std::string> event_to_correlation_;
    std::map<std::string, std::vector<std::string>> correlation_to_events_;
};

} // namespace nexus::utility::eventsystem
