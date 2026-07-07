#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>

namespace nexus::utility::reversing {

/**
 * @brief Track detections of debugger-evasion / anti-analysis techniques.
 */
class DebuggerEvasionDetector {
public:
    enum class Technique {
        IsDebuggerPresent,
        PtraceSelf,
        TimingCheck,
        HardwareBreakpoint,
        IntThreeScan,
        ParentProcessCheck,
        VmDetection,
        Other
    };

    struct Detection {
        Technique technique;
        std::string detail;
        std::size_t count = 0;
    };

    static DebuggerEvasionDetector& instance() {
        static DebuggerEvasionDetector inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    void recordDetection(Technique technique, const std::string& detail = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        auto& d = detections_[technique];
        d.technique = technique;
        if (!detail.empty()) d.detail = detail;
        ++d.count;
        ++total_;
    }

    std::size_t countFor(Technique technique) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = detections_.find(technique);
        return it == detections_.end() ? 0 : it->second.count;
    }

    std::size_t totalDetections() const { return total_.load(); }

    std::vector<Detection> allDetections() const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::vector<Detection> out;
        for (const auto& [t, d] : detections_) out.push_back(d);
        return out;
    }

    bool evasionSuspected() const { return total_.load() > 0; }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        detections_.clear();
        total_ = 0;
    }

private:
    DebuggerEvasionDetector() = default;
    ~DebuggerEvasionDetector() = default;
    DebuggerEvasionDetector(const DebuggerEvasionDetector&) = delete;
    DebuggerEvasionDetector& operator=(const DebuggerEvasionDetector&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::unordered_map<Technique, Detection> detections_;
    std::atomic<std::size_t> total_{0};
};

} // namespace nexus::utility::reversing
