#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <cstdint>

namespace nexus::utility::binary {

/**
 * @brief Register expected magic bytes per format and identify buffers.
 */
class MagicNumberValidator {
public:
    struct MagicSignature {
        std::string format;
        std::vector<std::uint8_t> magic;
        std::size_t offset = 0;
    };

    static MagicNumberValidator& instance() {
        static MagicNumberValidator inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        registerBuiltins();
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    void registerFormat(const std::string& format, const std::vector<std::uint8_t>& magic,
                        std::size_t offset = 0) {
        std::lock_guard<std::mutex> lk(mutex_);
        signatures_[format] = {format, magic, offset};
    }

    static bool matches(const MagicSignature& sig, const std::uint8_t* data, std::size_t size) {
        if (sig.offset + sig.magic.size() > size) return false;
        for (std::size_t i = 0; i < sig.magic.size(); ++i)
            if (data[sig.offset + i] != sig.magic[i]) return false;
        return true;
    }

    bool validate(const std::string& format, const std::uint8_t* data, std::size_t size) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = signatures_.find(format);
        return it != signatures_.end() && matches(it->second, data, size);
    }

    /// Identify a buffer's format by testing all registered signatures.
    std::string identify(const std::uint8_t* data, std::size_t size) const {
        std::lock_guard<std::mutex> lk(mutex_);
        for (const auto& [fmt, sig] : signatures_)
            if (matches(sig, data, size)) return fmt;
        return "";
    }

    std::size_t formatCount() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return signatures_.size();
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        signatures_.clear();
    }

private:
    MagicNumberValidator() = default;
    ~MagicNumberValidator() = default;
    MagicNumberValidator(const MagicNumberValidator&) = delete;
    MagicNumberValidator& operator=(const MagicNumberValidator&) = delete;

    void registerBuiltins() {
        signatures_["PNG"]  = {"PNG",  {0x89, 0x50, 0x4E, 0x47}, 0};
        signatures_["JPEG"] = {"JPEG", {0xFF, 0xD8, 0xFF}, 0};
        signatures_["GIF"]  = {"GIF",  {0x47, 0x49, 0x46, 0x38}, 0};
        signatures_["PDF"]  = {"PDF",  {0x25, 0x50, 0x44, 0x46}, 0};
        signatures_["ELF"]  = {"ELF",  {0x7F, 0x45, 0x4C, 0x46}, 0};
        signatures_["ZIP"]  = {"ZIP",  {0x50, 0x4B, 0x03, 0x04}, 0};
    }

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::unordered_map<std::string, MagicSignature> signatures_;
};

} // namespace nexus::utility::binary
