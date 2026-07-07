#pragma once

#include <string>
#include <unordered_map>
#include <mutex>

namespace nexus::utility::cpp23 {

/// @brief Track usage of the C++23 "deducing this" pattern across classes.
class DeducingThisHelper {
public:
    static DeducingThisHelper& instance() {
        static DeducingThisHelper inst;
        return inst;
    }

    void initialize() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = true;
    }

    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
        usage_.clear();
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    void recordUsage(const std::string& class_name, const std::string& method) {
        std::lock_guard<std::mutex> lock(mutex_);
        ++usage_[class_name].total;
        ++usage_[class_name].methods[method];
    }

    /// Total number of deducing-this usages recorded for a class.
    size_t getUsageCount(const std::string& class_name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = usage_.find(class_name);
        return it != usage_.end() ? it->second.total : 0;
    }

    /// Name of the class with the most recorded usages ("" if none).
    std::string getMostUsed() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string best;
        size_t best_count = 0;
        for (const auto& [name, info] : usage_) {
            if (info.total > best_count) {
                best_count = info.total;
                best = name;
            }
        }
        return best;
    }

private:
    struct ClassUsage {
        size_t total = 0;
        std::unordered_map<std::string, size_t> methods;
    };

    DeducingThisHelper() = default;
    ~DeducingThisHelper() = default;
    DeducingThisHelper(const DeducingThisHelper&) = delete;
    DeducingThisHelper& operator=(const DeducingThisHelper&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::unordered_map<std::string, ClassUsage> usage_;
};

} // namespace nexus::utility::cpp23
