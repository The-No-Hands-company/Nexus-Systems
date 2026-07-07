#pragma once

#include <cstddef>
#include <map>
#include <mutex>
#include <string>

namespace nexus::utility::eventsystem {

/// @brief Inspects message queues by tracking enqueue/dequeue counts and depth.
class MessageQueueInspector {
public:
    struct QueueStats {
        size_t capacity = 0;
        size_t enqueued = 0;
        size_t dequeued = 0;
        size_t current = 0;
    };

    static MessageQueueInspector& instance() {
        static MessageQueueInspector inst;
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
        queues_.clear();
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    void registerQueue(const std::string& name, size_t capacity) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto& q = queues_[name];
        q.capacity = capacity;
    }

    void recordEnqueue(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto& q = queues_[name];
        ++q.enqueued;
        ++q.current;
    }

    void recordDequeue(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto& q = queues_[name];
        ++q.dequeued;
        if (q.current > 0) --q.current;
    }

    size_t getQueueDepth(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = queues_.find(name);
        return it != queues_.end() ? it->second.current : 0;
    }

    QueueStats getQueueStats(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = queues_.find(name);
        return it != queues_.end() ? it->second : QueueStats{};
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        queues_.clear();
    }

private:
    MessageQueueInspector() = default;
    ~MessageQueueInspector() = default;
    MessageQueueInspector(const MessageQueueInspector&) = delete;
    MessageQueueInspector& operator=(const MessageQueueInspector&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::map<std::string, QueueStats> queues_;
};

} // namespace nexus::utility::eventsystem
