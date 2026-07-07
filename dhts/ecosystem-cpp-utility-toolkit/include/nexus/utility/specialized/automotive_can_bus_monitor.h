#pragma once

#include <string>
#include <vector>
#include <array>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <cstdint>

namespace nexus::utility::specialized {

/**
 * @brief Monitor automotive CAN bus messages, grouped by arbitration ID.
 */
class AutomotiveCanBusMonitor {
public:
    struct CanMessage {
        std::uint32_t id = 0;          // arbitration ID (11 or 29 bit)
        std::array<std::uint8_t, 8> data{};
        std::uint8_t dlc = 0;          // data length code
        std::uint64_t timestampUs = 0;
        bool extended = false;
    };

    struct IdStats {
        std::uint32_t id = 0;
        std::size_t count = 0;
        std::uint64_t firstSeenUs = 0;
        std::uint64_t lastSeenUs = 0;
        CanMessage lastMessage;
    };

    static AutomotiveCanBusMonitor& instance() {
        static AutomotiveCanBusMonitor inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    void recordMessage(const CanMessage& msg) {
        std::lock_guard<std::mutex> lk(mutex_);
        auto& s = byId_[msg.id];
        if (s.count == 0) {
            s.id = msg.id;
            s.firstSeenUs = msg.timestampUs;
        }
        ++s.count;
        s.lastSeenUs = msg.timestampUs;
        s.lastMessage = msg;
        ++total_;
    }

    IdStats stats(std::uint32_t id) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = byId_.find(id);
        return it == byId_.end() ? IdStats{id, 0, 0, 0, {}} : it->second;
    }

    /// Estimated message period in microseconds for an ID (0 if insufficient data).
    std::uint64_t estimatedPeriodUs(std::uint32_t id) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = byId_.find(id);
        if (it == byId_.end() || it->second.count < 2) return 0;
        return (it->second.lastSeenUs - it->second.firstSeenUs) / (it->second.count - 1);
    }

    std::vector<IdStats> allIds() const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::vector<IdStats> out;
        for (const auto& [id, s] : byId_) out.push_back(s);
        return out;
    }

    std::size_t totalMessages() const { return total_.load(); }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        byId_.clear();
        total_ = 0;
    }

private:
    AutomotiveCanBusMonitor() = default;
    ~AutomotiveCanBusMonitor() = default;
    AutomotiveCanBusMonitor(const AutomotiveCanBusMonitor&) = delete;
    AutomotiveCanBusMonitor& operator=(const AutomotiveCanBusMonitor&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::unordered_map<std::uint32_t, IdStats> byId_;
    std::atomic<std::size_t> total_{0};
};

} // namespace nexus::utility::specialized
