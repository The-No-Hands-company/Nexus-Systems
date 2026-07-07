#pragma once

#include <cstddef>
#include <limits>
#include <map>
#include <mutex>
#include <string>

namespace nexus::utility::eventsystem {

/// @brief Debugs event-bus publish/delivery activity with per-event-type statistics.
class EventBusDebugger {
public:
    struct EventStats {
        size_t publish_count = 0;
        size_t total_subscribers = 0;
        size_t delivery_count = 0;
        double avg_latency_us = 0.0;
        double min_latency_us = 0.0;
        double max_latency_us = 0.0;
    };

    struct DeliveryStats {
        size_t total_deliveries = 0;
        double avg_latency_us = 0.0;
        double max_latency_us = 0.0;
    };

    static EventBusDebugger& instance() {
        static EventBusDebugger inst;
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
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    void recordPublish(const std::string& event_type, size_t subscriber_count) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto& e = events_[event_type];
        ++e.publish_count;
        e.total_subscribers += subscriber_count;
    }

    void recordDelivery(const std::string& event_type, const std::string& subscriber, double latency_us) {
        std::lock_guard<std::mutex> lock(mutex_);
        (void)subscriber;
        auto& e = events_[event_type];
        if (e.delivery_count == 0) {
            e.min_latency_us = latency_us;
            e.max_latency_us = latency_us;
        } else {
            if (latency_us < e.min_latency_us) e.min_latency_us = latency_us;
            if (latency_us > e.max_latency_us) e.max_latency_us = latency_us;
        }
        ++e.delivery_count;
        e.total_latency_us += latency_us;
    }

    EventStats getEventStats(const std::string& event_type) const {
        std::lock_guard<std::mutex> lock(mutex_);
        EventStats out;
        auto it = events_.find(event_type);
        if (it != events_.end()) {
            const auto& e = it->second;
            out.publish_count = e.publish_count;
            out.total_subscribers = e.total_subscribers;
            out.delivery_count = e.delivery_count;
            out.avg_latency_us = e.delivery_count > 0
                                     ? e.total_latency_us / static_cast<double>(e.delivery_count)
                                     : 0.0;
            out.min_latency_us = e.delivery_count > 0 ? e.min_latency_us : 0.0;
            out.max_latency_us = e.max_latency_us;
        }
        return out;
    }

    DeliveryStats getDeliveryStats() const {
        std::lock_guard<std::mutex> lock(mutex_);
        DeliveryStats out;
        double total_latency = 0.0;
        for (const auto& [_, e] : events_) {
            out.total_deliveries += e.delivery_count;
            total_latency += e.total_latency_us;
            if (e.delivery_count > 0 && e.max_latency_us > out.max_latency_us) {
                out.max_latency_us = e.max_latency_us;
            }
        }
        out.avg_latency_us = out.total_deliveries > 0
                                 ? total_latency / static_cast<double>(out.total_deliveries)
                                 : 0.0;
        return out;
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        events_.clear();
    }

private:
    struct EventData {
        size_t publish_count = 0;
        size_t total_subscribers = 0;
        size_t delivery_count = 0;
        double total_latency_us = 0.0;
        double min_latency_us = 0.0;
        double max_latency_us = 0.0;
    };

    EventBusDebugger() = default;
    ~EventBusDebugger() = default;
    EventBusDebugger(const EventBusDebugger&) = delete;
    EventBusDebugger& operator=(const EventBusDebugger&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::map<std::string, EventData> events_;
};

} // namespace nexus::utility::eventsystem
