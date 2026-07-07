#pragma once

#include <string>
#include <map>
#include <mutex>

namespace nexus::utility::cloud {

class GcpErrorHandler {
public:
    static GcpErrorHandler& instance() {
        static GcpErrorHandler inst;
        return inst;
    }

    void initialize(const std::string& config = "") { std::lock_guard<std::mutex> lk(mutex_); enabled_ = true; config_ = config; }
    void shutdown() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = false; }
    bool isEnabled() const { return enabled_; }
    void enable() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = true; }
    void disable() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = false; }
    void reset() { std::lock_guard<std::mutex> lk(mutex_); last_error_.clear(); }

    std::string translateError(const std::string& code) {
        std::lock_guard<std::mutex> lk(mutex_);
        last_error_ = code;
        static const std::map<std::string, std::string> errors = {
            {"PERMISSION_DENIED", "Insufficient permissions to access the resource"},
            {"NOT_FOUND", "The requested resource was not found"},
            {"ALREADY_EXISTS", "The resource already exists"},
            {"RESOURCE_EXHAUSTED", "Quota exceeded or resource limit reached"},
            {"FAILED_PRECONDITION", "Operation was rejected because the system is not in the required state"},
            {"ABORTED", "Operation was aborted, typically due to a concurrency issue"},
            {"OUT_OF_RANGE", "Operation was attempted past the valid range"},
            {"UNAUTHENTICATED", "Request lacks valid authentication credentials"},
            {"UNAVAILABLE", "Service is currently unavailable, retry with backoff"},
            {"DEADLINE_EXCEEDED", "Request deadline exceeded before completion"},
            {"INTERNAL", "Internal server error, retry may succeed"},
            {"INVALID_ARGUMENT", "Client specified an invalid argument"},
            {"CANCELLED", "Operation was cancelled by the caller"},
            {"DATA_LOSS", "Unrecoverable data loss or corruption"},
            {"RATE_LIMIT_EXCEEDED", "Request rate limit exceeded, implement backoff"},
            {"BILLING_DISABLED", "Billing is not enabled for this project"},
            {"QUOTA_EXCEEDED", "Project quota has been exceeded"},
        };
        auto it = errors.find(code);
        if (it != errors.end()) return it->second;
        auto cit = custom_errors_.find(code);
        if (cit != custom_errors_.end()) return cit->second;
        return "Unknown GCP error: " + code;
    }

    void addCustomError(const std::string& code, const std::string& msg) {
        std::lock_guard<std::mutex> lk(mutex_);
        custom_errors_[code] = msg;
    }

    std::string getLastError() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return last_error_;
    }

private:
    GcpErrorHandler() = default;
    ~GcpErrorHandler() = default;
    GcpErrorHandler(const GcpErrorHandler&) = delete;
    GcpErrorHandler& operator=(const GcpErrorHandler&) = delete;
    GcpErrorHandler(GcpErrorHandler&&) = delete;
    GcpErrorHandler& operator=(GcpErrorHandler&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::string last_error_;
    std::map<std::string, std::string> custom_errors_;
};

} // namespace nexus::utility::cloud
