#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <chrono>
#include <cstdint>

namespace nexus::utility::embedded {

class BootloaderDiagnostics {
public:
    struct BootRecord {
        std::string version;
        bool success;
        uint64_t timestamp; // epoch ms
    };

    static BootloaderDiagnostics& instance() {
        static BootloaderDiagnostics inst;
        return inst;
    }

    void initialize() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = true; }
    void shutdown() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = false; }
    bool isEnabled() const { return enabled_; }

    void recordBootAttempt(const std::string& version, bool success) {
        std::lock_guard<std::mutex> lk(mutex_);
        if (!enabled_) return;
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        records_.push_back({version, success, static_cast<uint64_t>(now)});
        if (success) success_count_++;
    }

    double getSuccessRate() const {
        std::lock_guard<std::mutex> lk(mutex_);
        if (records_.empty()) return 0.0;
        return static_cast<double>(success_count_) / records_.size() * 100.0;
    }

    size_t getBootCount() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return records_.size();
    }

    uint64_t getLastBootTime() const {
        std::lock_guard<std::mutex> lk(mutex_);
        if (records_.empty()) return 0;
        return records_.back().timestamp;
    }

private:
    BootloaderDiagnostics() = default;
    ~BootloaderDiagnostics() = default;
    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::vector<BootRecord> records_;
    size_t success_count_ = 0;
};

} // namespace nexus::utility::embedded
