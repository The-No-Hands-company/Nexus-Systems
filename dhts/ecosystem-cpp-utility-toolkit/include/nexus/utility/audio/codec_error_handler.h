#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>

namespace nexus::utility::audio {

/**
 * @brief Track and categorize audio codec errors.
 */
class CodecErrorHandler {
public:
    enum class ErrorType {
        DecodeFailure,
        EncodeFailure,
        UnsupportedFormat,
        CorruptStream,
        BufferOverflow,
        SyncLost,
        Other
    };

    struct ErrorRecord {
        ErrorType type;
        std::string codec;
        std::string message;
    };

    static CodecErrorHandler& instance() {
        static CodecErrorHandler inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    void recordError(ErrorType type, const std::string& codec, const std::string& message = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        history_.push_back({type, codec, message});
        ++countByType_[type];
        ++total_;
    }

    std::size_t countFor(ErrorType type) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = countByType_.find(type);
        return it == countByType_.end() ? 0 : it->second;
    }

    std::size_t totalErrors() const { return total_.load(); }

    /// True if errors are frequent enough to consider the stream unhealthy.
    bool isRecoverable() const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = countByType_.find(ErrorType::CorruptStream);
        std::size_t corrupt = it == countByType_.end() ? 0 : it->second;
        return corrupt < 10;
    }

    std::vector<ErrorRecord> history() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return history_;
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        history_.clear();
        countByType_.clear();
        total_ = 0;
    }

private:
    CodecErrorHandler() = default;
    ~CodecErrorHandler() = default;
    CodecErrorHandler(const CodecErrorHandler&) = delete;
    CodecErrorHandler& operator=(const CodecErrorHandler&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::vector<ErrorRecord> history_;
    std::unordered_map<ErrorType, std::size_t> countByType_;
    std::atomic<std::size_t> total_{0};
};

} // namespace nexus::utility::audio
