#pragma once

#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <chrono>
#include <algorithm>
#include <shared_mutex>
#include <mutex>
#include <ostream>

namespace nexus::utility::event {

/// @brief Generic event bus for publish-subscribe pattern
template<typename EventType>
class EventBus {
public:
    using EventHandler = std::function<void(const EventType&)>;
    using HandlerId = size_t;

    HandlerId subscribe(EventHandler handler);
    void unsubscribe(HandlerId id);

    void publish(const EventType& event);

    void clear();
    size_t subscriberCount() const;

private:
    HandlerId next_id_ = 0;
    std::unordered_map<HandlerId, EventHandler> handlers_;
    mutable std::shared_mutex mutex_;
};

/// @brief Message queue with priority
template<typename MessageType>
class MessageQueue {
public:
    enum class Priority {
        Low = 0,
        Normal = 1,
        High = 2,
        Critical = 3
    };

    void enqueue(MessageType message, Priority priority = Priority::Normal);
    bool dequeue(MessageType& message_out);

    bool isEmpty() const;
    size_t size() const;
    void clear();

private:
    struct QueuedMessage {
        MessageType message;
        Priority priority;
        size_t sequence_number;
    };

    size_t sequence_ = 0;
    std::vector<QueuedMessage> queue_;
    mutable std::mutex mutex_;
};

/// @brief Event logger for debugging
class EventLogger {
public:
    void logEvent(const std::string& event_name, const std::string& details = "");

    struct EventRecord {
        std::chrono::steady_clock::time_point timestamp;
        std::string event_name;
        std::string details;
    };

    const std::vector<EventRecord>& getLog() const;
    void printLog(std::ostream& os, size_t max_events = 100) const;
    void clear();

private:
    std::vector<EventRecord> log_;
};

// Template implementations

template<typename EventType>
typename EventBus<EventType>::HandlerId EventBus<EventType>::subscribe(EventHandler handler) {
    std::unique_lock lock(mutex_);
    HandlerId id = next_id_++;
    handlers_[id] = std::move(handler);
    return id;
}

template<typename EventType>
void EventBus<EventType>::unsubscribe(HandlerId id) {
    std::unique_lock lock(mutex_);
    handlers_.erase(id);
}

template<typename EventType>
void EventBus<EventType>::publish(const EventType& event) {
    std::shared_lock lock(mutex_);
    for (const auto& [id, handler] : handlers_) {
        handler(event);
    }
}

template<typename EventType>
void EventBus<EventType>::clear() {
    std::unique_lock lock(mutex_);
    handlers_.clear();
}

template<typename EventType>
size_t EventBus<EventType>::subscriberCount() const {
    std::shared_lock lock(mutex_);
    return handlers_.size();
}

template<typename MessageType>
void MessageQueue<MessageType>::enqueue(MessageType message, Priority priority) {
    std::lock_guard lock(mutex_);
    queue_.push_back({std::move(message), priority, sequence_++});

    std::sort(queue_.begin(), queue_.end(),
        [](const QueuedMessage& a, const QueuedMessage& b) {
            if (a.priority != b.priority) {
                return static_cast<int>(a.priority) > static_cast<int>(b.priority);
            }
            return a.sequence_number < b.sequence_number;
        });
}

template<typename MessageType>
bool MessageQueue<MessageType>::dequeue(MessageType& message_out) {
    std::lock_guard lock(mutex_);
    if (queue_.empty()) {
        return false;
    }

    message_out = std::move(queue_.front().message);
    queue_.erase(queue_.begin());
    return true;
}

template<typename MessageType>
bool MessageQueue<MessageType>::isEmpty() const {
    std::lock_guard lock(mutex_);
    return queue_.empty();
}

template<typename MessageType>
size_t MessageQueue<MessageType>::size() const {
    std::lock_guard lock(mutex_);
    return queue_.size();
}

template<typename MessageType>
void MessageQueue<MessageType>::clear() {
    std::lock_guard lock(mutex_);
    queue_.clear();
    sequence_ = 0;
}

} // namespace nexus::utility::event
