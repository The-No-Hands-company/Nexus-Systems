#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <cstdint>

namespace nexus::utility::reversing {

/**
 * @brief Register expected code signatures and validate binaries against them.
 */
class CodeSignatureValidator {
public:
    struct Signature {
        std::string name;
        std::vector<std::uint8_t> pattern;
        std::vector<bool> mask;   // true = must match, false = wildcard
    };

    static CodeSignatureValidator& instance() {
        static CodeSignatureValidator inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    void registerSignature(const std::string& name,
                           const std::vector<std::uint8_t>& pattern,
                           const std::vector<bool>& mask = {}) {
        std::lock_guard<std::mutex> lk(mutex_);
        std::vector<bool> m = mask.empty() ? std::vector<bool>(pattern.size(), true) : mask;
        signatures_[name] = {name, pattern, m};
    }

    static bool matchAt(const Signature& sig, const std::uint8_t* data, std::size_t size,
                        std::size_t pos) {
        if (pos + sig.pattern.size() > size) return false;
        for (std::size_t i = 0; i < sig.pattern.size(); ++i) {
            if (sig.mask[i] && data[pos + i] != sig.pattern[i]) return false;
        }
        return true;
    }

    /// Find the first offset where a named signature matches, or -1 if absent.
    std::int64_t find(const std::string& name, const std::uint8_t* data, std::size_t size) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = signatures_.find(name);
        if (it == signatures_.end()) return -1;
        for (std::size_t pos = 0; pos + it->second.pattern.size() <= size; ++pos)
            if (matchAt(it->second, data, size, pos)) return static_cast<std::int64_t>(pos);
        return -1;
    }

    bool validate(const std::string& name, const std::uint8_t* data, std::size_t size) {
        bool ok = find(name, data, size) >= 0;
        std::lock_guard<std::mutex> lk(mutex_);
        ++validations_;
        if (!ok) ++mismatches_;
        return ok;
    }

    std::size_t signatureCount() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return signatures_.size();
    }
    std::size_t mismatches() const { return mismatches_.load(); }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        signatures_.clear();
        validations_ = 0;
        mismatches_ = 0;
    }

private:
    CodeSignatureValidator() = default;
    ~CodeSignatureValidator() = default;
    CodeSignatureValidator(const CodeSignatureValidator&) = delete;
    CodeSignatureValidator& operator=(const CodeSignatureValidator&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::unordered_map<std::string, Signature> signatures_;
    std::atomic<std::size_t> validations_{0};
    std::atomic<std::size_t> mismatches_{0};
};

} // namespace nexus::utility::reversing
