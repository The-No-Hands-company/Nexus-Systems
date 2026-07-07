#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <utility>

namespace nexus::utility::codegen {

/// @brief Debug the preprocessor stringification operator (#).
class TokenStringificationDebugger {
public:
    using Entry = std::pair<std::string, std::string>; // token -> result

    static TokenStringificationDebugger& instance() {
        static TokenStringificationDebugger inst;
        return inst;
    }

    void initialize() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = true;
    }

    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
        entries_.clear();
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    void recordStringify(const std::string& token, const std::string& result) {
        std::lock_guard<std::mutex> lock(mutex_);
        entries_.emplace_back(token, result);
    }

    std::vector<Entry> getStringifications() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return entries_;
    }

    size_t getTokenCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return entries_.size();
    }

private:
    TokenStringificationDebugger() = default;
    ~TokenStringificationDebugger() = default;
    TokenStringificationDebugger(const TokenStringificationDebugger&) = delete;
    TokenStringificationDebugger& operator=(const TokenStringificationDebugger&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::vector<Entry> entries_;
};

} // namespace nexus::utility::codegen
