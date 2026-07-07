#pragma once

#include <string>
#include <vector>
#include <deque>
#include <atomic>
#include <mutex>
#include <chrono>
#include <cstdint>

namespace nexus::utility::protocol {

/**
 * @brief Log captured packet data with timestamps (in-memory capture buffer).
 */
class PacketCaptureLogger {
public:
    enum class Direction { Inbound, Outbound };

    struct Packet {
        std::uint64_t timestampUs = 0;
        Direction direction = Direction::Inbound;
        std::string source;
        std::string destination;
        std::vector<std::uint8_t> data;
        std::size_t size() const { return data.size(); }
    };

    static PacketCaptureLogger& instance() {
        static PacketCaptureLogger inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    void setCaptureLimit(std::size_t limit) {
        std::lock_guard<std::mutex> lk(mutex_);
        limit_ = limit;
    }

    void capture(Direction dir, const std::string& src, const std::string& dst,
                 const std::vector<std::uint8_t>& data) {
        std::uint64_t ts = nowUs();
        std::lock_guard<std::mutex> lk(mutex_);
        packets_.push_back({ts, dir, src, dst, data});
        while (packets_.size() > limit_) packets_.pop_front();
        ++total_;
        totalBytes_ += data.size();
    }

    std::vector<Packet> packets() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return {packets_.begin(), packets_.end()};
    }

    std::size_t totalCaptured() const { return total_.load(); }
    std::size_t totalBytes() const { return totalBytes_.load(); }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        packets_.clear();
        total_ = 0;
        totalBytes_ = 0;
    }

private:
    PacketCaptureLogger() = default;
    ~PacketCaptureLogger() = default;
    PacketCaptureLogger(const PacketCaptureLogger&) = delete;
    PacketCaptureLogger& operator=(const PacketCaptureLogger&) = delete;

    static std::uint64_t nowUs() {
        return static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
    }

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::size_t limit_ = 1024;
    std::deque<Packet> packets_;
    std::atomic<std::size_t> total_{0};
    std::atomic<std::size_t> totalBytes_{0};
};

} // namespace nexus::utility::protocol
