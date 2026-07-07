#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <chrono>
#include <cstdint>

namespace nexus::utility::specialized {

/**
 * @brief Log cryptographic operations with algorithm and key-size metadata.
 */
class CryptographicOperationLogger {
public:
    enum class Operation { Encrypt, Decrypt, Sign, Verify, Hash, KeyGen, KeyExchange };

    struct LogEntry {
        Operation operation;
        std::string algorithm;
        std::size_t keyBits = 0;
        std::size_t dataBytes = 0;
        bool success = true;
        std::uint64_t timestampMs = 0;
    };

    static CryptographicOperationLogger& instance() {
        static CryptographicOperationLogger inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    void log(Operation op, const std::string& algorithm, std::size_t keyBits,
             std::size_t dataBytes, bool success = true) {
        auto ts = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        std::lock_guard<std::mutex> lk(mutex_);
        entries_.push_back({op, algorithm, keyBits, dataBytes, success,
                            static_cast<std::uint64_t>(ts)});
        ++opCounts_[algorithm];
        if (!success) ++failures_;
    }

    /// Flag algorithms/key sizes considered weak by modern standards.
    bool isWeak(const std::string& algorithm, std::size_t keyBits) const {
        if (algorithm == "MD5" || algorithm == "SHA1" || algorithm == "DES" ||
            algorithm == "RC4") return true;
        if (algorithm == "RSA" && keyBits < 2048) return true;
        if (algorithm == "AES" && keyBits < 128) return true;
        return false;
    }

    std::size_t countFor(const std::string& algorithm) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = opCounts_.find(algorithm);
        return it == opCounts_.end() ? 0 : it->second;
    }

    std::size_t failures() const { return failures_.load(); }

    std::vector<LogEntry> entries() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return entries_;
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        entries_.clear();
        opCounts_.clear();
        failures_ = 0;
    }

private:
    CryptographicOperationLogger() = default;
    ~CryptographicOperationLogger() = default;
    CryptographicOperationLogger(const CryptographicOperationLogger&) = delete;
    CryptographicOperationLogger& operator=(const CryptographicOperationLogger&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::vector<LogEntry> entries_;
    std::unordered_map<std::string, std::size_t> opCounts_;
    std::atomic<std::size_t> failures_{0};
};

} // namespace nexus::utility::specialized
