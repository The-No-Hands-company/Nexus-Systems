#pragma once

#include <cstdint>
#include <mutex>
#include <string>

namespace nexus::utility::hardware {

class EccErrorMonitor {
public:
    static EccErrorMonitor& instance() {
        static EccErrorMonitor inst;
        return inst;
    }

    void initialize(const std::string& = "") { std::lock_guard<std::mutex> lock(mutex_); enabled_ = true; }
    void shutdown() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = false; reset(); }
    bool isEnabled() const { return enabled_; }
    void enable() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = true; }
    void disable() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = false; }

    std::string getStatus() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return "EccErrorMonitor: correctable=" + std::to_string(correctable_) +
               " uncorrectable=" + std::to_string(uncorrectable_) +
               ", enabled=" + (enabled_ ? "true" : "false");
    }
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        correctable_ = 0;
        uncorrectable_ = 0;
    }

    void recordCorrectableError(uint64_t) {
        std::lock_guard<std::mutex> lock(mutex_);
        ++correctable_;
    }

    void recordUncorrectableError(uint64_t) {
        std::lock_guard<std::mutex> lock(mutex_);
        ++uncorrectable_;
    }

    uint64_t getCorrectableCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return correctable_;
    }

    uint64_t getUncorrectableCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return uncorrectable_;
    }

    uint64_t getTotalErrors() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return correctable_ + uncorrectable_;
    }

private:
    EccErrorMonitor() = default;
    ~EccErrorMonitor() = default;
    EccErrorMonitor(const EccErrorMonitor&) = delete;
    EccErrorMonitor& operator=(const EccErrorMonitor&) = delete;
    EccErrorMonitor(EccErrorMonitor&&) = delete;
    EccErrorMonitor& operator=(EccErrorMonitor&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    uint64_t correctable_ = 0;
    uint64_t uncorrectable_ = 0;
};

} // namespace nexus::utility::hardware
