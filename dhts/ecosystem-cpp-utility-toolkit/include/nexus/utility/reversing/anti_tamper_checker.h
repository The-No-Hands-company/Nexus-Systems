#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <cstdint>

namespace nexus::utility::reversing {

/**
 * @brief Detect code/data tampering by tracking checksums of protected regions.
 */
class AntiTamperChecker {
public:
    struct Region {
        std::string name;
        std::uint64_t baselineChecksum = 0;
        std::uint64_t currentChecksum = 0;
        bool tampered = false;
    };

    static AntiTamperChecker& instance() {
        static AntiTamperChecker inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    /// Simple FNV-1a checksum over a memory region.
    static std::uint64_t checksum(const void* data, std::size_t size) {
        const auto* bytes = static_cast<const std::uint8_t*>(data);
        std::uint64_t h = 1469598103934665603ULL;
        for (std::size_t i = 0; i < size; ++i) {
            h ^= bytes[i];
            h *= 1099511628211ULL;
        }
        return h;
    }

    void registerRegion(const std::string& name, const void* data, std::size_t size) {
        std::uint64_t cs = checksum(data, size);
        std::lock_guard<std::mutex> lk(mutex_);
        regions_[name] = {name, cs, cs, false};
    }

    /// Recompute checksum and flag tampering if it changed from the baseline.
    bool verify(const std::string& name, const void* data, std::size_t size) {
        std::uint64_t cs = checksum(data, size);
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = regions_.find(name);
        if (it == regions_.end()) return false;
        it->second.currentChecksum = cs;
        it->second.tampered = (cs != it->second.baselineChecksum);
        if (it->second.tampered) ++detections_;
        return !it->second.tampered;
    }

    bool anyTampered() const {
        std::lock_guard<std::mutex> lk(mutex_);
        for (const auto& [n, r] : regions_) if (r.tampered) return true;
        return false;
    }

    std::size_t detections() const { return detections_.load(); }

    std::vector<Region> tamperedRegions() const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::vector<Region> out;
        for (const auto& [n, r] : regions_) if (r.tampered) out.push_back(r);
        return out;
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        regions_.clear();
        detections_ = 0;
    }

private:
    AntiTamperChecker() = default;
    ~AntiTamperChecker() = default;
    AntiTamperChecker(const AntiTamperChecker&) = delete;
    AntiTamperChecker& operator=(const AntiTamperChecker&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::unordered_map<std::string, Region> regions_;
    std::atomic<std::size_t> detections_{0};
};

} // namespace nexus::utility::reversing
