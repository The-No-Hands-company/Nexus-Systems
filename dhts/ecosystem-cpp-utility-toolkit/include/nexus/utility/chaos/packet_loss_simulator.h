#pragma once

#include <string>
#include <atomic>
#include <random>

namespace nexus::utility::chaos {

class PacketLossSimulator {
public:
    static PacketLossSimulator& instance() {
        static PacketLossSimulator inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        config_ = config;
        enabled_ = true;
    }

    void shutdown() {
        reset();
        enabled_ = false;
    }

    bool isEnabled() const { return enabled_; }
    void enable() { enabled_ = true; }
    void disable() { enabled_ = false; }

    std::string getStatus() const {
        return "loss rate: " + std::to_string(loss_rate_.load()) + "%";
    }

    void reset() {
        loss_rate_ = 0.0;
    }

    void setLossRate(double percent) {
        loss_rate_ = percent;
    }

    double getLossRate() const {
        return loss_rate_.load();
    }

    bool shouldDrop() {
        if (!enabled_) return false;
        thread_local std::mt19937 rng(std::random_device{}());
        thread_local std::uniform_real_distribution<double> dist(0.0, 100.0);
        return dist(rng) < loss_rate_.load();
    }

private:
    PacketLossSimulator() = default;
    ~PacketLossSimulator() = default;

    PacketLossSimulator(const PacketLossSimulator&) = delete;
    PacketLossSimulator& operator=(const PacketLossSimulator&) = delete;
    PacketLossSimulator(PacketLossSimulator&&) = delete;
    PacketLossSimulator& operator=(PacketLossSimulator&&) = delete;

    bool enabled_ = false;
    std::string config_;
    std::atomic<double> loss_rate_{0.0};
};

} // namespace nexus::utility::chaos
