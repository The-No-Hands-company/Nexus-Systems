#pragma once

#include <map>
#include <mutex>
#include <cstdint>
#include <limits>

namespace nexus::utility::embedded {

class InterruptLatencyTracker {
public:
    struct IrqStats {
        size_t count = 0;
        double max = 0.0;
        double total = 0.0;
    };

    static InterruptLatencyTracker& instance() {
        static InterruptLatencyTracker inst;
        return inst;
    }

    void initialize() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = true; }
    void shutdown() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = false; }
    bool isEnabled() const { return enabled_; }

    void recordInterrupt(uint8_t irq, double latency_us) {
        std::lock_guard<std::mutex> lk(mutex_);
        if (!enabled_) return;
        auto& s = irqs_[irq];
        s.count++;
        s.total += latency_us;
        if (latency_us > s.max) s.max = latency_us;
    }

    double getMaxLatency(uint8_t irq) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = irqs_.find(irq);
        return (it != irqs_.end()) ? it->second.max : 0.0;
    }

    double getAvgLatency(uint8_t irq) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = irqs_.find(irq);
        if (it == irqs_.end() || it->second.count == 0) return 0.0;
        return it->second.total / it->second.count;
    }

    size_t getInterruptCount(uint8_t irq) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = irqs_.find(irq);
        return (it != irqs_.end()) ? it->second.count : 0;
    }

private:
    InterruptLatencyTracker() = default;
    ~InterruptLatencyTracker() = default;
    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::map<uint8_t, IrqStats> irqs_;
};

} // namespace nexus::utility::embedded
