#pragma once

#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <chrono>
#include <cstdint>

namespace nexus::utility::protocol {

/**
 * @brief Log protocol handshake steps with per-step timing.
 */
class HandshakeTracer {
public:
    struct Step {
        std::string name;
        std::string direction;   // "send" / "recv"
        std::uint64_t timestampUs = 0;
        double elapsedSinceStartUs = 0.0;
        bool success = true;
    };

    static HandshakeTracer& instance() {
        static HandshakeTracer inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    void begin() {
        std::lock_guard<std::mutex> lk(mutex_);
        steps_.clear();
        start_ = nowUs();
        active_ = true;
    }

    void step(const std::string& name, const std::string& direction, bool success = true) {
        std::uint64_t ts = nowUs();
        std::lock_guard<std::mutex> lk(mutex_);
        if (!active_) { start_ = ts; active_ = true; }
        Step s;
        s.name = name;
        s.direction = direction;
        s.timestampUs = ts;
        s.elapsedSinceStartUs = static_cast<double>(ts - start_);
        s.success = success;
        steps_.push_back(s);
        if (!success) ++failures_;
    }

    void complete() {
        std::lock_guard<std::mutex> lk(mutex_);
        active_ = false;
    }

    /// Total handshake duration in microseconds.
    double durationUs() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return steps_.empty() ? 0.0 : steps_.back().elapsedSinceStartUs;
    }

    std::vector<Step> steps() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return steps_;
    }

    std::size_t failures() const { return failures_.load(); }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        steps_.clear();
        active_ = false;
        failures_ = 0;
    }

private:
    HandshakeTracer() = default;
    ~HandshakeTracer() = default;
    HandshakeTracer(const HandshakeTracer&) = delete;
    HandshakeTracer& operator=(const HandshakeTracer&) = delete;

    static std::uint64_t nowUs() {
        return static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count());
    }

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::vector<Step> steps_;
    std::uint64_t start_ = 0;
    bool active_ = false;
    std::atomic<std::size_t> failures_{0};
};

} // namespace nexus::utility::protocol
