#pragma once

#include <string>
#include <unordered_map>
#include <mutex>

namespace nexus::utility::codegen {

/// @brief Help debug variadic template/macro pack expansions.
class VariadicExpanderHelper {
public:
    static VariadicExpanderHelper& instance() {
        static VariadicExpanderHelper inst;
        return inst;
    }

    void initialize() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = true;
    }

    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
        expansions_.clear();
        total_ = 0;
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    /// Record that a pack expanded into `count` elements.
    void recordExpansion(const std::string& pack_name, size_t count) {
        std::lock_guard<std::mutex> lock(mutex_);
        expansions_[pack_name] += count;
        total_ += count;
    }

    /// Total elements produced across all expansions of a pack.
    size_t getExpansionCount(const std::string& pack_name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = expansions_.find(pack_name);
        return it != expansions_.end() ? it->second : 0;
    }

    /// Total elements produced across all packs.
    size_t getTotalExpansions() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return total_;
    }

private:
    VariadicExpanderHelper() = default;
    ~VariadicExpanderHelper() = default;
    VariadicExpanderHelper(const VariadicExpanderHelper&) = delete;
    VariadicExpanderHelper& operator=(const VariadicExpanderHelper&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::unordered_map<std::string, size_t> expansions_;
    size_t total_ = 0;
};

} // namespace nexus::utility::codegen
