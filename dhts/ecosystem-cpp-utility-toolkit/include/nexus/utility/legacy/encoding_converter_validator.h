#pragma once

#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <cstdint>

namespace nexus::utility::legacy {

/**
 * @brief Validate character-encoding conversions (UTF-8 well-formedness, round-trip).
 */
class EncodingConverterValidator {
public:
    static EncodingConverterValidator& instance() {
        static EncodingConverterValidator inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    /// Validate that a byte sequence is well-formed UTF-8.
    static bool isValidUtf8(const std::uint8_t* data, std::size_t size) {
        std::size_t i = 0;
        while (i < size) {
            std::uint8_t c = data[i];
            std::size_t extra = 0;
            if (c < 0x80) extra = 0;
            else if ((c >> 5) == 0x6) extra = 1;
            else if ((c >> 4) == 0xE) extra = 2;
            else if ((c >> 3) == 0x1E) extra = 3;
            else return false;
            if (i + extra >= size && extra > 0) return false;
            for (std::size_t j = 1; j <= extra; ++j)
                if ((data[i + j] & 0xC0) != 0x80) return false;
            i += extra + 1;
        }
        return true;
    }

    bool validateUtf8(const std::string& text) {
        bool ok = isValidUtf8(reinterpret_cast<const std::uint8_t*>(text.data()), text.size());
        std::lock_guard<std::mutex> lk(mutex_);
        ++validations_;
        if (!ok) ++failures_;
        return ok;
    }

    /// Verify a conversion round-trips back to the original bytes.
    bool validateRoundTrip(const std::string& original, const std::string& roundTripped) {
        bool ok = (original == roundTripped);
        std::lock_guard<std::mutex> lk(mutex_);
        ++validations_;
        if (!ok) ++failures_;
        return ok;
    }

    /// Count Unicode code points in a UTF-8 string (assumes valid input).
    static std::size_t codePointCount(const std::string& text) {
        std::size_t count = 0;
        for (unsigned char c : text) if ((c & 0xC0) != 0x80) ++count;
        return count;
    }

    std::size_t validations() const { return validations_.load(); }
    std::size_t failures() const { return failures_.load(); }

    void reset() { validations_ = 0; failures_ = 0; }

private:
    EncodingConverterValidator() = default;
    ~EncodingConverterValidator() = default;
    EncodingConverterValidator(const EncodingConverterValidator&) = delete;
    EncodingConverterValidator& operator=(const EncodingConverterValidator&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::atomic<std::size_t> validations_{0};
    std::atomic<std::size_t> failures_{0};
};

} // namespace nexus::utility::legacy
