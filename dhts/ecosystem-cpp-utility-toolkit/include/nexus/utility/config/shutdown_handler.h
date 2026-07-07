#pragma once

#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <algorithm>

namespace nexus::utility::config {

class ShutdownHandler {
public:
    struct HandlerEntry {
        std::string name;
        std::function<void()> handler;
        int priority;
    };

    static ShutdownHandler& instance() {
        static ShutdownHandler inst;
        return inst;
    }

    void initialize() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = true; }
    void shutdown() {
        std::lock_guard<std::mutex> lk(mutex_);
        if (!enabled_) return;
        std::sort(handlers_.begin(), handlers_.end(),
            [](const HandlerEntry& a, const HandlerEntry& b) { return a.priority < b.priority; });
        for (auto& h : handlers_) {
            if (h.handler) h.handler();
        }
        enabled_ = false;
    }
    bool isEnabled() const { return enabled_; }

    void registerHandler(const std::string& name, std::function<void()> handler, int priority) {
        std::lock_guard<std::mutex> lk(mutex_);
        handlers_.push_back({name, std::move(handler), priority});
    }

    size_t getHandlerCount() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return handlers_.size();
    }

private:
    ShutdownHandler() = default;
    ~ShutdownHandler() = default;
    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::vector<HandlerEntry> handlers_;
};

} // namespace nexus::utility::config
