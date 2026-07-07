#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>

namespace nexus::utility::codegen {

/// @brief Check macro hygiene: detect name collisions between macros.
class MacroHygieneChecker {
public:
    static MacroHygieneChecker& instance() {
        static MacroHygieneChecker inst;
        return inst;
    }

    void initialize() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = true;
    }

    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
        macros_.clear();
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    void registerMacro(const std::string& name, const std::string& expansion) {
        std::lock_guard<std::mutex> lock(mutex_);
        macros_[name].push_back(expansion);
    }

    /// Return the differing expansions registered for a name (collisions).
    /// Empty if the macro is undefined or consistently defined once.
    std::vector<std::string> checkCollision(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = macros_.find(name);
        if (it == macros_.end()) return {};
        std::vector<std::string> distinct;
        for (const auto& exp : it->second) {
            bool seen = false;
            for (const auto& d : distinct) {
                if (d == exp) { seen = true; break; }
            }
            if (!seen) distinct.push_back(exp);
        }
        if (distinct.size() <= 1) return {};
        return distinct;
    }

    size_t getMacroCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return macros_.size();
    }

private:
    MacroHygieneChecker() = default;
    ~MacroHygieneChecker() = default;
    MacroHygieneChecker(const MacroHygieneChecker&) = delete;
    MacroHygieneChecker& operator=(const MacroHygieneChecker&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::unordered_map<std::string, std::vector<std::string>> macros_;
};

} // namespace nexus::utility::codegen
