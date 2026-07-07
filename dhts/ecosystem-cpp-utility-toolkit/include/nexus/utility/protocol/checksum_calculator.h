#pragma once

#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <cstdint>

namespace nexus::utility::protocol {

/**
 * @brief Compute and verify common checksums for protocol frames.
 */
class ChecksumCalculator {
public:
    static ChecksumCalculator& instance() {
        static ChecksumCalculator inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    /// Internet checksum (RFC 1071), used by IP/TCP/UDP.
    static std::uint16_t internetChecksum(const std::uint8_t* data, std::size_t size) {
        std::uint32_t sum = 0;
        for (std::size_t i = 0; i + 1 < size; i += 2)
            sum += (static_cast<std::uint32_t>(data[i]) << 8) | data[i + 1];
        if (size & 1) sum += static_cast<std::uint32_t>(data[size - 1]) << 8;
        while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
        return static_cast<std::uint16_t>(~sum);
    }

    /// CRC-32 (IEEE 802.3).
    static std::uint32_t crc32(const std::uint8_t* data, std::size_t size) {
        std::uint32_t crc = 0xFFFFFFFF;
        for (std::size_t i = 0; i < size; ++i) {
            crc ^= data[i];
            for (int j = 0; j < 8; ++j)
                crc = (crc >> 1) ^ (0xEDB88320 & (~((crc & 1) - 1)));
        }
        return ~crc;
    }

    /// Simple 8-bit additive checksum.
    static std::uint8_t sum8(const std::uint8_t* data, std::size_t size) {
        std::uint8_t s = 0;
        for (std::size_t i = 0; i < size; ++i) s = static_cast<std::uint8_t>(s + data[i]);
        return s;
    }

    std::uint32_t compute(const std::uint8_t* data, std::size_t size) {
        std::lock_guard<std::mutex> lk(mutex_);
        ++computed_;
        return crc32(data, size);
    }

    bool verify(const std::uint8_t* data, std::size_t size, std::uint32_t expected) {
        std::uint32_t actual = crc32(data, size);
        std::lock_guard<std::mutex> lk(mutex_);
        ++verified_;
        if (actual != expected) { ++failures_; return false; }
        return true;
    }

    std::size_t failures() const { return failures_.load(); }

    void reset() { computed_ = 0; verified_ = 0; failures_ = 0; }

private:
    ChecksumCalculator() = default;
    ~ChecksumCalculator() = default;
    ChecksumCalculator(const ChecksumCalculator&) = delete;
    ChecksumCalculator& operator=(const ChecksumCalculator&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::atomic<std::size_t> computed_{0};
    std::atomic<std::size_t> verified_{0};
    std::atomic<std::size_t> failures_{0};
};

} // namespace nexus::utility::protocol
