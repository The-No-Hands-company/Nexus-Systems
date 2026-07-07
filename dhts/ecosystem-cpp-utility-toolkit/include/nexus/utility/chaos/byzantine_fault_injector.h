#pragma once

#include <string>
#include <unordered_map>
#include <mutex>
#include <optional>

namespace nexus::utility::chaos {

class ByzantineFaultInjector {
public:
    enum class CorruptionType {
        FlipBits,
        ReturnNull,
        ReturnGarbage,
        Timeout
    };

    static ByzantineFaultInjector& instance() {
        static ByzantineFaultInjector inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        config_ = config;
        enabled_ = true;
    }

    void shutdown() {
        reset();
        enabled_ = false;
    }

    bool isEnabled() const { return enabled_; }
    void enable() { enabled_ = true; }
    void disable() { enabled_ = false; }

    std::string getStatus() const {
        std::lock_guard lock(mutex_);
        return std::to_string(rules_.size()) + " corrupt rules";
    }

    void reset() {
        std::lock_guard lock(mutex_);
        rules_.clear();
    }

    void addCorruptRule(const std::string& operation, CorruptionType corruption_type) {
        std::lock_guard lock(mutex_);
        rules_[operation] = corruption_type;
    }

    void removeCorruptRule(const std::string& operation) {
        std::lock_guard lock(mutex_);
        rules_.erase(operation);
    }

    std::optional<CorruptionType> shouldCorrupt(const std::string& operation) const {
        if (!enabled_) return std::nullopt;
        std::lock_guard lock(mutex_);
        auto it = rules_.find(operation);
        if (it == rules_.end()) return std::nullopt;
        return it->second;
    }

private:
    ByzantineFaultInjector() = default;
    ~ByzantineFaultInjector() = default;

    ByzantineFaultInjector(const ByzantineFaultInjector&) = delete;
    ByzantineFaultInjector& operator=(const ByzantineFaultInjector&) = delete;
    ByzantineFaultInjector(ByzantineFaultInjector&&) = delete;
    ByzantineFaultInjector& operator=(ByzantineFaultInjector&&) = delete;

    bool enabled_ = false;
    std::string config_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, CorruptionType> rules_;
};

} // namespace nexus::utility::chaos
