#pragma once

#include <functional>
#include <atomic>
#include <mutex>
#include <string>

namespace nexus::utility::cloud {

class SpotInstanceHandler {
public:
    static SpotInstanceHandler& instance() {
        static SpotInstanceHandler inst;
        return inst;
    }

    void initialize(const std::string& config = "") { std::lock_guard<std::mutex> lk(mutex_); enabled_ = true; config_ = config; }
    void shutdown() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = false; }
    bool isEnabled() const { return enabled_; }
    void enable() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = true; }
    void disable() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = false; }
    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        interrupted_.store(false);
        interruption_count_.store(0);
    }

    void setInterruptionHandler(std::function<void()> handler) {
        std::lock_guard<std::mutex> lk(mutex_);
        handler_ = std::move(handler);
    }

    void simulateInterruption() {
        std::lock_guard<std::mutex> lk(mutex_);
        interrupted_.store(true);
        interruption_count_++;
        if (handler_) handler_();
    }

    size_t getInterruptionCount() const {
        return interruption_count_.load();
    }

    bool isInterrupted() const {
        return interrupted_.load();
    }

private:
    SpotInstanceHandler() = default;
    ~SpotInstanceHandler() = default;
    SpotInstanceHandler(const SpotInstanceHandler&) = delete;
    SpotInstanceHandler& operator=(const SpotInstanceHandler&) = delete;
    SpotInstanceHandler(SpotInstanceHandler&&) = delete;
    SpotInstanceHandler& operator=(SpotInstanceHandler&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::function<void()> handler_;
    std::atomic<bool> interrupted_{false};
    std::atomic<size_t> interruption_count_{0};
};

} // namespace nexus::utility::cloud
