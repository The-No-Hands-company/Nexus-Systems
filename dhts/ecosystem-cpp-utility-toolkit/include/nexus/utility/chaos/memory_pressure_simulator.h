#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace nexus::utility::chaos {

class MemoryPressureSimulator {
public:
    static MemoryPressureSimulator& instance() {
        static MemoryPressureSimulator inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        config_ = config;
        enabled_ = true;
    }

    void shutdown() {
        releasePressure();
        enabled_ = false;
    }

    bool isEnabled() const { return enabled_; }
    void enable() { enabled_ = true; }
    void disable() { enabled_ = false; }

    std::string getStatus() const {
        return "pressure: " + std::to_string(pressure_mb_) + " MB";
    }

    void reset() {
        releasePressure();
    }

    void applyPressure(size_t megabytes) {
        releasePressure();
        if (megabytes == 0) return;
        size_t bytes = megabytes * 1024ULL * 1024ULL;
        memory_block_.resize(bytes);
        std::fill(memory_block_.begin(), memory_block_.end(), static_cast<char>(0));
        pressure_mb_ = megabytes;
    }

    void releasePressure() {
        memory_block_.clear();
        memory_block_.shrink_to_fit();
        pressure_mb_ = 0;
    }

    size_t getPressureMB() const { return pressure_mb_; }

private:
    MemoryPressureSimulator() = default;
    ~MemoryPressureSimulator() { releasePressure(); }

    MemoryPressureSimulator(const MemoryPressureSimulator&) = delete;
    MemoryPressureSimulator& operator=(const MemoryPressureSimulator&) = delete;
    MemoryPressureSimulator(MemoryPressureSimulator&&) = delete;
    MemoryPressureSimulator& operator=(MemoryPressureSimulator&&) = delete;

    bool enabled_ = false;
    std::string config_;
    size_t pressure_mb_ = 0;
    std::vector<char> memory_block_;
};

} // namespace nexus::utility::chaos
