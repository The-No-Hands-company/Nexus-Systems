#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>

namespace nexus::utility::workflow {

/**
 * @brief Detect breaking changes between two API surface snapshots.
 */
class ApiBreakingChangeDetector {
public:
    enum class ChangeKind { Removed, SignatureChanged, Added, Deprecated };

    struct Change {
        std::string symbol;
        ChangeKind kind;
        std::string detail;
        bool breaking = false;
    };

    struct ApiSymbol {
        std::string name;
        std::string signature;
    };

    static ApiBreakingChangeDetector& instance() {
        static ApiBreakingChangeDetector inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    void addBaselineSymbol(const std::string& name, const std::string& signature) {
        std::lock_guard<std::mutex> lk(mutex_);
        baseline_[name] = {name, signature};
    }

    void addCurrentSymbol(const std::string& name, const std::string& signature) {
        std::lock_guard<std::mutex> lk(mutex_);
        current_[name] = {name, signature};
    }

    /// Compare baseline vs current; returns all detected changes.
    std::vector<Change> detect() {
        std::lock_guard<std::mutex> lk(mutex_);
        std::vector<Change> changes;
        for (const auto& [name, sym] : baseline_) {
            auto it = current_.find(name);
            if (it == current_.end()) {
                changes.push_back({name, ChangeKind::Removed, "symbol removed", true});
            } else if (it->second.signature != sym.signature) {
                changes.push_back({name, ChangeKind::SignatureChanged,
                                   sym.signature + " -> " + it->second.signature, true});
            }
        }
        for (const auto& [name, sym] : current_)
            if (!baseline_.count(name))
                changes.push_back({name, ChangeKind::Added, "new symbol", false});
        breakingCount_ = 0;
        for (const auto& c : changes) if (c.breaking) ++breakingCount_;
        return changes;
    }

    bool hasBreakingChanges() {
        for (const auto& c : detect()) if (c.breaking) return true;
        return false;
    }

    std::size_t breakingCount() const { return breakingCount_.load(); }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        baseline_.clear();
        current_.clear();
        breakingCount_ = 0;
    }

private:
    ApiBreakingChangeDetector() = default;
    ~ApiBreakingChangeDetector() = default;
    ApiBreakingChangeDetector(const ApiBreakingChangeDetector&) = delete;
    ApiBreakingChangeDetector& operator=(const ApiBreakingChangeDetector&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::unordered_map<std::string, ApiSymbol> baseline_;
    std::unordered_map<std::string, ApiSymbol> current_;
    std::atomic<std::size_t> breakingCount_{0};
};

} // namespace nexus::utility::workflow
