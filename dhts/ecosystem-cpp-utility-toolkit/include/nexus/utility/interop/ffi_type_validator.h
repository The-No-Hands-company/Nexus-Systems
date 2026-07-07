#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>

namespace nexus::utility::interop {

/**
 * @brief Validate FFI type mappings between languages/ABIs.
 */
class FfiTypeValidator {
public:
    struct TypeMapping {
        std::string sourceType;   // e.g. C++ "int32_t"
        std::string targetType;   // e.g. Python "c_int32"
        std::size_t sourceSize = 0;
        std::size_t targetSize = 0;
        bool compatible = true;
    };

    static FfiTypeValidator& instance() {
        static FfiTypeValidator inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    void registerMapping(const std::string& sourceType, const std::string& targetType,
                         std::size_t sourceSize, std::size_t targetSize) {
        std::lock_guard<std::mutex> lk(mutex_);
        bool ok = (sourceSize == targetSize);
        mappings_[sourceType] = {sourceType, targetType, sourceSize, targetSize, ok};
    }

    /// A mapping is valid if the source type is registered and sizes match.
    bool validate(const std::string& sourceType) {
        std::lock_guard<std::mutex> lk(mutex_);
        ++validations_;
        auto it = mappings_.find(sourceType);
        if (it == mappings_.end()) { ++failures_; return false; }
        if (!it->second.compatible) ++failures_;
        return it->second.compatible;
    }

    /// Direct size-compatibility check for two types.
    bool sizesMatch(std::size_t sourceSize, std::size_t targetSize) {
        std::lock_guard<std::mutex> lk(mutex_);
        ++validations_;
        bool ok = sourceSize == targetSize;
        if (!ok) ++failures_;
        return ok;
    }

    std::vector<TypeMapping> incompatibleMappings() const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::vector<TypeMapping> out;
        for (const auto& [t, m] : mappings_) if (!m.compatible) out.push_back(m);
        return out;
    }

    std::size_t validations() const { return validations_.load(); }
    std::size_t failures() const { return failures_.load(); }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        mappings_.clear();
        validations_ = 0;
        failures_ = 0;
    }

private:
    FfiTypeValidator() = default;
    ~FfiTypeValidator() = default;
    FfiTypeValidator(const FfiTypeValidator&) = delete;
    FfiTypeValidator& operator=(const FfiTypeValidator&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::unordered_map<std::string, TypeMapping> mappings_;
    std::atomic<std::size_t> validations_{0};
    std::atomic<std::size_t> failures_{0};
};

} // namespace nexus::utility::interop
