#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <chrono>
#include <cstdint>

namespace nexus::utility::cloud {

class AzureDiagnosticsHelper {
public:
    struct LogEntry {
        std::string source;
        std::string message;
        uint64_t timestamp;
    };

    static AzureDiagnosticsHelper& instance() {
        static AzureDiagnosticsHelper inst;
        return inst;
    }

    void initialize(const std::string& config = "") { std::lock_guard<std::mutex> lk(mutex_); enabled_ = true; config_ = config; }
    void shutdown() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = false; }
    bool isEnabled() const { return enabled_; }
    void enable() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = true; }
    void disable() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = false; }
    void reset() { std::lock_guard<std::mutex> lk(mutex_); logs_.clear(); }

    void setConnectionString(const std::string& cs) {
        std::lock_guard<std::mutex> lk(mutex_);
        connection_string_ = cs;
    }

    void logDiagnostic(const std::string& source, const std::string& message) {
        std::lock_guard<std::mutex> lk(mutex_);
        if (!enabled_) return;
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        logs_.push_back({source, message, static_cast<uint64_t>(now)});
    }

    std::vector<LogEntry> getLogs(const std::string& source) const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::vector<LogEntry> result;
        for (const auto& entry : logs_) {
            if (entry.source == source) result.push_back(entry);
        }
        return result;
    }

    void clear() {
        std::lock_guard<std::mutex> lk(mutex_);
        logs_.clear();
    }

private:
    AzureDiagnosticsHelper() = default;
    ~AzureDiagnosticsHelper() = default;
    AzureDiagnosticsHelper(const AzureDiagnosticsHelper&) = delete;
    AzureDiagnosticsHelper& operator=(const AzureDiagnosticsHelper&) = delete;
    AzureDiagnosticsHelper(AzureDiagnosticsHelper&&) = delete;
    AzureDiagnosticsHelper& operator=(AzureDiagnosticsHelper&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::string connection_string_;
    std::vector<LogEntry> logs_;
};

} // namespace nexus::utility::cloud
