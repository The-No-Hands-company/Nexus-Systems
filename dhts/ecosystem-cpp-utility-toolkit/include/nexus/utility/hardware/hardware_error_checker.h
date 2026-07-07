#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace nexus::utility::hardware {

class HardwareErrorChecker {
public:
    struct ErrorEntry {
        std::string component;
        std::string error_type;
        int severity;
        std::chrono::system_clock::time_point timestamp;
    };

    static HardwareErrorChecker& instance() {
        static HardwareErrorChecker inst;
        return inst;
    }

    void initialize(const std::string& = "") { std::lock_guard<std::mutex> lock(mutex_); enabled_ = true; }
    void shutdown() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = false; reset(); }
    bool isEnabled() const { return enabled_; }
    void enable() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = true; }
    void disable() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = false; }

    std::string getStatus() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return "HardwareErrorChecker: " + std::to_string(errors_.size()) + " errors, " +
               std::to_string(component_errors_.size()) + " components, enabled=" + (enabled_ ? "true" : "false");
    }
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        errors_.clear();
        component_errors_.clear();
    }

    void recordError(const std::string& component, const std::string& error_type, int severity) {
        std::lock_guard<std::mutex> lock(mutex_);
        errors_.push_back({component, error_type, severity, std::chrono::system_clock::now()});
        component_errors_[component].push_back(errors_.back());
    }

    std::vector<ErrorEntry> getErrors(const std::string& component) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = component_errors_.find(component);
        if (it != component_errors_.end()) return it->second;
        return {};
    }

    std::vector<ErrorEntry> getErrorsBySeverity(int severity) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<ErrorEntry> result;
        for (const auto& e : errors_) {
            if (e.severity == severity) result.push_back(e);
        }
        return result;
    }

    uint64_t getTotalErrors() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return errors_.size();
    }

    uint64_t getComponentCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return component_errors_.size();
    }

private:
    HardwareErrorChecker() = default;
    ~HardwareErrorChecker() = default;
    HardwareErrorChecker(const HardwareErrorChecker&) = delete;
    HardwareErrorChecker& operator=(const HardwareErrorChecker&) = delete;
    HardwareErrorChecker(HardwareErrorChecker&&) = delete;
    HardwareErrorChecker& operator=(HardwareErrorChecker&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::vector<ErrorEntry> errors_;
    std::map<std::string, std::vector<ErrorEntry>> component_errors_;
};

} // namespace nexus::utility::hardware
