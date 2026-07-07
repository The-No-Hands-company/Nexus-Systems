#pragma once

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <chrono>
#include <cstdint>

namespace nexus::utility::apivalidation {

class ApiCallRecorder {
public:
    struct CallRecord {
        std::string function;
        std::string args;
        uint64_t timestamp;
    };

    static ApiCallRecorder& instance() {
        static ApiCallRecorder inst;
        return inst;
    }

    void initialize(const std::string& config = "") { std::lock_guard<std::mutex> lk(mutex_); enabled_ = true; config_ = config; }
    void shutdown() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = false; }
    bool isEnabled() const { return enabled_; }
    void enable() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = true; }
    void disable() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = false; }
    void reset() { std::lock_guard<std::mutex> lk(mutex_); records_.clear(); call_counts_.clear(); }

    void recordCall(const std::string& function, const std::string& args) {
        std::lock_guard<std::mutex> lk(mutex_);
        if (!enabled_) return;
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        records_.push_back({function, args, static_cast<uint64_t>(now)});
        call_counts_[function]++;
    }

    size_t getCallCount(const std::string& function) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = call_counts_.find(function);
        return (it != call_counts_.end()) ? it->second : 0;
    }

    std::vector<CallRecord> getCallHistory() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return records_;
    }

    size_t getTotalCalls() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return records_.size();
    }

private:
    ApiCallRecorder() = default;
    ~ApiCallRecorder() = default;
    ApiCallRecorder(const ApiCallRecorder&) = delete;
    ApiCallRecorder& operator=(const ApiCallRecorder&) = delete;
    ApiCallRecorder(ApiCallRecorder&&) = delete;
    ApiCallRecorder& operator=(ApiCallRecorder&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::vector<CallRecord> records_;
    std::map<std::string, size_t> call_counts_;
};

} // namespace nexus::utility::apivalidation
