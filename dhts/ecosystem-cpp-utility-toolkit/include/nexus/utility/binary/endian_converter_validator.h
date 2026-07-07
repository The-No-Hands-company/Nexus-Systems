#pragma once

#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <cstdint>
#include <cstring>

namespace nexus::utility::binary {

/**
 * @brief Convert between endianness and validate round-trip conversions.
 */
class EndianConverterValidator {
public:
    enum class Endian { Little, Big };

    static EndianConverterValidator& instance() {
        static EndianConverterValidator inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    static Endian nativeEndian() {
        std::uint16_t v = 1;
        return (*reinterpret_cast<std::uint8_t*>(&v) == 1) ? Endian::Little : Endian::Big;
    }

    static std::uint16_t swap16(std::uint16_t v) {
        return static_cast<std::uint16_t>((v >> 8) | (v << 8));
    }
    static std::uint32_t swap32(std::uint32_t v) {
        return ((v >> 24) & 0x000000FF) | ((v >> 8) & 0x0000FF00) |
               ((v << 8) & 0x00FF0000) | ((v << 24) & 0xFF000000);
    }
    static std::uint64_t swap64(std::uint64_t v) {
        return ((v & 0x00000000000000FFULL) << 56) |
               ((v & 0x000000000000FF00ULL) << 40) |
               ((v & 0x0000000000FF0000ULL) << 24) |
               ((v & 0x00000000FF000000ULL) << 8)  |
               ((v & 0x000000FF00000000ULL) >> 8)  |
               ((v & 0x0000FF0000000000ULL) >> 24) |
               ((v & 0x00FF000000000000ULL) >> 40) |
               ((v & 0xFF00000000000000ULL) >> 56);
    }

    template<typename T>
    T toEndian(T value, Endian target) {
        if (target == nativeEndian()) return value;
        std::lock_guard<std::mutex> lk(mutex_);
        ++conversions_;
        if constexpr (sizeof(T) == 2) { auto r = swap16(static_cast<std::uint16_t>(value)); return static_cast<T>(r); }
        else if constexpr (sizeof(T) == 4) { auto r = swap32(static_cast<std::uint32_t>(value)); return static_cast<T>(r); }
        else if constexpr (sizeof(T) == 8) { auto r = swap64(static_cast<std::uint64_t>(value)); return static_cast<T>(r); }
        else return value;
    }

    /// Verify that converting to target endian and back yields the original.
    template<typename T>
    bool validateRoundTrip(T value, Endian target) {
        T converted = toEndian(value, target);
        T back = toEndian(converted, target);
        bool ok = (back == value);
        std::lock_guard<std::mutex> lk(mutex_);
        if (!ok) ++failures_;
        return ok;
    }

    std::size_t conversions() const { return conversions_.load(); }
    std::size_t failures() const { return failures_.load(); }

    void reset() { conversions_ = 0; failures_ = 0; }

private:
    EndianConverterValidator() = default;
    ~EndianConverterValidator() = default;
    EndianConverterValidator(const EndianConverterValidator&) = delete;
    EndianConverterValidator& operator=(const EndianConverterValidator&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::atomic<std::size_t> conversions_{0};
    std::atomic<std::size_t> failures_{0};
};

} // namespace nexus::utility::binary
