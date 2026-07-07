#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <cstdint>

namespace nexus::utility::binary {

/**
 * @brief Verify that a struct's actual size matches an expected packed size.
 */
class StructPackingValidator {
public:
    struct StructInfo {
        std::string name;
        std::size_t actualSize = 0;
        std::size_t expectedSize = 0;
        std::size_t sumOfFields = 0;
        bool matches = true;
    };

    static StructPackingValidator& instance() {
        static StructPackingValidator inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    /// Register a struct with its actual sizeof and expected packed size.
    bool check(const std::string& name, std::size_t actualSize, std::size_t expectedSize,
               std::size_t sumOfFields = 0) {
        bool ok = (actualSize == expectedSize);
        std::lock_guard<std::mutex> lk(mutex_);
        structs_[name] = {name, actualSize, expectedSize, sumOfFields, ok};
        if (!ok) ++mismatches_;
        return ok;
    }

    /// Padding overhead = actual size - sum of field sizes.
    std::size_t paddingOverhead(const std::string& name) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = structs_.find(name);
        if (it == structs_.end() || it->second.sumOfFields == 0) return 0;
        return it->second.actualSize > it->second.sumOfFields
            ? it->second.actualSize - it->second.sumOfFields : 0;
    }

    bool isPacked(const std::string& name) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = structs_.find(name);
        return it != structs_.end() && it->second.sumOfFields > 0 &&
               it->second.actualSize == it->second.sumOfFields;
    }

    std::vector<StructInfo> mismatchedStructs() const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::vector<StructInfo> out;
        for (const auto& [n, s] : structs_) if (!s.matches) out.push_back(s);
        return out;
    }

    std::size_t mismatches() const { return mismatches_.load(); }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        structs_.clear();
        mismatches_ = 0;
    }

private:
    StructPackingValidator() = default;
    ~StructPackingValidator() = default;
    StructPackingValidator(const StructPackingValidator&) = delete;
    StructPackingValidator& operator=(const StructPackingValidator&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::unordered_map<std::string, StructInfo> structs_;
    std::atomic<std::size_t> mismatches_{0};
};

} // namespace nexus::utility::binary
