#pragma once

#include <string>
#include <atomic>
#include <mutex>
#include <cstdint>

namespace nexus::utility::crossplatform {

/**
 * @brief Detect the byte order (endianness) of the host platform.
 */
class EndiannessDetector {
public:
    enum class Endian { Little, Big, Mixed };

    static EndiannessDetector& instance() {
        static EndiannessDetector inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    /// Detect endianness at runtime.
    static Endian detect() {
        std::uint32_t v = 0x01020304;
        auto* p = reinterpret_cast<std::uint8_t*>(&v);
        if (p[0] == 0x04) return Endian::Little;
        if (p[0] == 0x01) return Endian::Big;
        return Endian::Mixed;
    }

    static bool isLittleEndian() { return detect() == Endian::Little; }
    static bool isBigEndian() { return detect() == Endian::Big; }

    static const char* name(Endian e) {
        switch (e) {
            case Endian::Little: return "little";
            case Endian::Big:    return "big";
            case Endian::Mixed:  return "mixed";
        }
        return "unknown";
    }

    std::string detectedName() const { return name(detect()); }

    void reset() {}

private:
    EndiannessDetector() = default;
    ~EndiannessDetector() = default;
    EndiannessDetector(const EndiannessDetector&) = delete;
    EndiannessDetector& operator=(const EndiannessDetector&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
};

} // namespace nexus::utility::crossplatform
