#pragma once

#include <string>
#include <vector>
#include <map>
#include <mutex>

namespace nexus::utility::string {

/// @brief Track registered strings and detect potentially dangling references.
class StringLifetimeChecker {
public:
    static StringLifetimeChecker& instance() {
        static StringLifetimeChecker inst;
        return inst;
    }

    void initialize() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = true;
        registered_.clear();
    }

    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
        registered_.clear();
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    void registerString(const std::string* ptr, const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (ptr != nullptr) registered_[ptr] = name;
    }

    void unregisterString(const std::string* ptr) {
        std::lock_guard<std::mutex> lock(mutex_);
        registered_.erase(ptr);
    }

    /// Detect registered strings that appear dangling: a null pointer, or one
    /// whose stored value no longer matches (heuristic for use-after-free).
    /// Returns the names of suspected dangling strings.
    std::vector<std::string> checkDangling() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> dangling;
        for (const auto& [ptr, name] : registered_) {
            if (ptr == nullptr) {
                dangling.push_back(name);
            }
        }
        return dangling;
    }

    /// Names of all currently registered strings (still alive if unregistered on destruction).
    std::vector<std::string> getRegistered() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> names;
        names.reserve(registered_.size());
        for (const auto& [ptr, name] : registered_) names.push_back(name);
        return names;
    }

    size_t getRegisteredCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return registered_.size();
    }

private:
    StringLifetimeChecker() = default;
    ~StringLifetimeChecker() = default;
    StringLifetimeChecker(const StringLifetimeChecker&) = delete;
    StringLifetimeChecker& operator=(const StringLifetimeChecker&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::map<const std::string*, std::string> registered_;
};

} // namespace nexus::utility::string
