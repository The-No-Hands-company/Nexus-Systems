#pragma once

#include <cstdint>
#include <mutex>
#include <string>

namespace nexus::utility::i18n {

class EncodingDetector {
public:
    static EncodingDetector& instance() {
        static EncodingDetector inst;
        return inst;
    }

    void initialize(const std::string& = "") { std::lock_guard<std::mutex> lock(mutex_); enabled_ = true; }
    void shutdown() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = false; }
    bool isEnabled() const { return enabled_; }
    void enable() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = true; }
    void disable() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = false; }
    std::string getStatus() const { return "EncodingDetector: enabled=" + std::string(enabled_ ? "true" : "false"); }
    void reset() {}

    std::string detect(const std::string& data) const {
        if (data.size() >= 3 && static_cast<unsigned char>(data[0]) == 0xEF &&
            static_cast<unsigned char>(data[1]) == 0xBB &&
            static_cast<unsigned char>(data[2]) == 0xBF) return "UTF-8";
        if (data.size() >= 2 && static_cast<unsigned char>(data[0]) == 0xFF &&
            static_cast<unsigned char>(data[1]) == 0xFE) return "UTF-16LE";
        if (data.size() >= 2 && static_cast<unsigned char>(data[0]) == 0xFE &&
            static_cast<unsigned char>(data[1]) == 0xFF) return "UTF-16BE";
        if (isValidUtf8(data)) return "UTF-8";
        return "ASCII";
    }

    bool isValidUtf8(const std::string& data) const {
        size_t i = 0;
        while (i < data.size()) {
            unsigned char c = static_cast<unsigned char>(data[i]);
            if (c <= 0x7F) {
                ++i;
            } else if (c >= 0xC2 && c <= 0xDF) {
                if (i + 1 >= data.size()) return false;
                if ((static_cast<unsigned char>(data[i + 1]) & 0xC0) != 0x80) return false;
                i += 2;
            } else if (c >= 0xE0 && c <= 0xEF) {
                if (i + 2 >= data.size()) return false;
                if ((static_cast<unsigned char>(data[i + 1]) & 0xC0) != 0x80) return false;
                if ((static_cast<unsigned char>(data[i + 2]) & 0xC0) != 0x80) return false;
                i += 3;
            } else if (c >= 0xF0 && c <= 0xF4) {
                if (i + 3 >= data.size()) return false;
                if ((static_cast<unsigned char>(data[i + 1]) & 0xC0) != 0x80) return false;
                if ((static_cast<unsigned char>(data[i + 2]) & 0xC0) != 0x80) return false;
                if ((static_cast<unsigned char>(data[i + 3]) & 0xC0) != 0x80) return false;
                i += 4;
            } else {
                return false;
            }
        }
        return true;
    }

    bool hasBOM(const std::string& data) const {
        if (data.size() >= 3 && static_cast<unsigned char>(data[0]) == 0xEF &&
            static_cast<unsigned char>(data[1]) == 0xBB &&
            static_cast<unsigned char>(data[2]) == 0xBF) return true;
        if (data.size() >= 2 && static_cast<unsigned char>(data[0]) == 0xFF &&
            static_cast<unsigned char>(data[1]) == 0xFE) return true;
        if (data.size() >= 2 && static_cast<unsigned char>(data[0]) == 0xFE &&
            static_cast<unsigned char>(data[1]) == 0xFF) return true;
        return false;
    }

private:
    EncodingDetector() = default;
    ~EncodingDetector() = default;
    EncodingDetector(const EncodingDetector&) = delete;
    EncodingDetector& operator=(const EncodingDetector&) = delete;
    EncodingDetector(EncodingDetector&&) = delete;
    EncodingDetector& operator=(EncodingDetector&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
};

} // namespace nexus::utility::i18n
