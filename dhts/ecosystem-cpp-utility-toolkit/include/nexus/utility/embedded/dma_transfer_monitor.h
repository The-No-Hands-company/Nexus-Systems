#pragma once

#include <map>
#include <mutex>
#include <cstdint>
#include <cstddef>

namespace nexus::utility::embedded {

class DmaTransferMonitor {
public:
    struct ChannelStats {
        size_t count = 0;
        size_t total_bytes = 0;
        double avg_time = 0.0;
    };

    static DmaTransferMonitor& instance() {
        static DmaTransferMonitor inst;
        return inst;
    }

    void initialize() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = true; }
    void shutdown() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = false; }
    bool isEnabled() const { return enabled_; }

    void recordTransfer(uint32_t channel, size_t bytes, double ms) {
        std::lock_guard<std::mutex> lk(mutex_);
        if (!enabled_) return;
        auto& s = channels_[channel];
        s.total_bytes += bytes;
        s.count++;
        s.avg_time = ((s.avg_time * (s.count - 1)) + ms) / s.count;
        total_transfers_++;
    }

    ChannelStats getChannelStats(uint32_t channel) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = channels_.find(channel);
        if (it != channels_.end()) return it->second;
        return {};
    }

    size_t getTotalTransfers() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return total_transfers_;
    }

private:
    DmaTransferMonitor() = default;
    ~DmaTransferMonitor() = default;
    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::map<uint32_t, ChannelStats> channels_;
    size_t total_transfers_ = 0;
};

} // namespace nexus::utility::embedded
