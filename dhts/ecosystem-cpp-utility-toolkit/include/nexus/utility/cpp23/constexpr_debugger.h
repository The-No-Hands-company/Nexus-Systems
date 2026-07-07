#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>

namespace nexus::utility::cpp23 {

/// @brief Track and debug constexpr evaluation of functions.
class ConstexprDebugger {
public:
    static ConstexprDebugger& instance() {
        static ConstexprDebugger inst;
        return inst;
    }

    void initialize() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = true;
    }

    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
        functions_.clear();
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    /// Mark a function as constexpr (or not).
    void markConstexpr(const std::string& function, bool is_constexpr) {
        std::lock_guard<std::mutex> lock(mutex_);
        functions_[function] = is_constexpr;
    }

    /// Returns true if the function is marked and evaluated as constexpr.
    bool checkConstexpr(const std::string& function) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = functions_.find(function);
        return it != functions_.end() && it->second;
    }

    /// Number of functions marked as constexpr.
    size_t getConstexprCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t count = 0;
        for (const auto& [name, is_ce] : functions_) {
            if (is_ce) ++count;
        }
        return count;
    }

    /// List of all tracked function names.
    std::vector<std::string> getFunctionList() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> names;
        names.reserve(functions_.size());
        for (const auto& [name, is_ce] : functions_) {
            names.push_back(name);
        }
        return names;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        functions_.clear();
    }

private:
    ConstexprDebugger() = default;
    ~ConstexprDebugger() = default;
    ConstexprDebugger(const ConstexprDebugger&) = delete;
    ConstexprDebugger& operator=(const ConstexprDebugger&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::unordered_map<std::string, bool> functions_;
};

} // namespace nexus::utility::cpp23
