#pragma once

#include <cstddef>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace nexus::utility::eventsystem {

/// @brief Detects subscriber leaks by tracking active subscriptions per event type.
class SubscriberLeakDetector {
public:
    static SubscriberLeakDetector& instance() {
        static SubscriberLeakDetector inst;
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
        subscribers_.clear();
        expected_max_.clear();
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    void registerSubscription(const std::string& event_type, void* subscriber) {
        std::lock_guard<std::mutex> lock(mutex_);
        subscribers_[event_type].insert(subscriber);
    }

    void unregisterSubscription(const std::string& event_type, void* subscriber) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = subscribers_.find(event_type);
        if (it != subscribers_.end()) {
            it->second.erase(subscriber);
        }
    }

    /// @brief Set the maximum expected number of subscribers for a type.
    void setExpectedMax(const std::string& event_type, size_t max_subscribers) {
        std::lock_guard<std::mutex> lock(mutex_);
        expected_max_[event_type] = max_subscribers;
    }

    size_t getSubscriberCount(const std::string& event_type) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = subscribers_.find(event_type);
        return it != subscribers_.end() ? it->second.size() : 0;
    }

    /// @brief Return event types whose active subscriber count exceeds the expected maximum.
    std::vector<std::string> detectLeaks() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> leaks;
        for (const auto& [type, subs] : subscribers_) {
            auto exp = expected_max_.find(type);
            if (exp != expected_max_.end() && subs.size() > exp->second) {
                leaks.push_back(type);
            }
        }
        return leaks;
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        subscribers_.clear();
        expected_max_.clear();
    }

private:
    SubscriberLeakDetector() = default;
    ~SubscriberLeakDetector() = default;
    SubscriberLeakDetector(const SubscriberLeakDetector&) = delete;
    SubscriberLeakDetector& operator=(const SubscriberLeakDetector&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::map<std::string, std::set<void*>> subscribers_;
    std::map<std::string, size_t> expected_max_;
};

} // namespace nexus::utility::eventsystem
