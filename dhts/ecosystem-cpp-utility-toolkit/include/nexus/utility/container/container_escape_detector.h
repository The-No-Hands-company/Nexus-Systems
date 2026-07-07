#pragma once

#include <string>
#include <set>
#include <vector>
#include <mutex>

namespace nexus::utility::container {

class ContainerEscapeDetector {
public:
    struct SuspiciousEvent {
        std::string type;
        std::string detail;
    };

    static ContainerEscapeDetector& instance() {
        static ContainerEscapeDetector inst;
        return inst;
    }

    void initialize(const std::string& config = "") { std::lock_guard<std::mutex> lk(mutex_); enabled_ = true; config_ = config; }
    void shutdown() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = false; }
    bool isEnabled() const { return enabled_; }
    void enable() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = true; }
    void disable() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = false; }
    void reset() { std::lock_guard<std::mutex> lk(mutex_); dangerous_caps_.clear(); suspicious_mounts_.clear(); events_.clear(); }

    void checkCapability(const std::string& capability) {
        std::lock_guard<std::mutex> lk(mutex_);
        if (!enabled_) return;
        static const std::set<std::string> dangerous = {
            "CAP_SYS_ADMIN", "CAP_SYS_PTRACE", "CAP_SYS_MODULE",
            "CAP_SYS_RAWIO", "CAP_SYS_BOOT", "CAP_NET_ADMIN",
            "CAP_DAC_OVERRIDE", "CAP_SYSLOG"
        };
        if (dangerous.find(capability) != dangerous.end()) {
            dangerous_caps_.insert(capability);
            events_.push_back({"capability", capability});
        }
    }

    void checkMount(const std::string& path) {
        std::lock_guard<std::mutex> lk(mutex_);
        if (!enabled_) return;
        static const std::set<std::string> dangerous_paths = {
            "/proc", "/sys", "/dev", "/", "/var/run/docker.sock",
            "/run/docker.sock", "/var/run/crio/crio.sock"
        };
        for (const auto& dp : dangerous_paths) {
            if (path.find(dp) != std::string::npos) {
                suspicious_mounts_.insert(path);
                events_.push_back({"mount", path});
                return;
            }
        }
    }

    bool isSuspicious() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return !events_.empty();
    }

    std::vector<SuspiciousEvent> getEvents() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return events_;
    }

private:
    ContainerEscapeDetector() = default;
    ~ContainerEscapeDetector() = default;
    ContainerEscapeDetector(const ContainerEscapeDetector&) = delete;
    ContainerEscapeDetector& operator=(const ContainerEscapeDetector&) = delete;
    ContainerEscapeDetector(ContainerEscapeDetector&&) = delete;
    ContainerEscapeDetector& operator=(ContainerEscapeDetector&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::set<std::string> dangerous_caps_;
    std::set<std::string> suspicious_mounts_;
    std::vector<SuspiciousEvent> events_;
};

} // namespace nexus::utility::container
