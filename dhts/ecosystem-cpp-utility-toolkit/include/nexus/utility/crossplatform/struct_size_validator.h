#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>

namespace nexus::utility::crossplatform {

/**
 * @brief Validate that struct sizes/alignments match across platforms/ABIs.
 */
class StructSizeValidator {
public:
    struct Expectation {
        std::string type;
        std::size_t expectedSize = 0;
        std::size_t expectedAlign = 0;
    };

    struct Result {
        std::string type;
        std::size_t actualSize = 0;
        std::size_t expectedSize = 0;
        std::size_t actualAlign = 0;
        std::size_t expectedAlign = 0;
        bool sizeOk = true;
        bool alignOk = true;
    };

    static StructSizeValidator& instance() {
        static StructSizeValidator inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    void expect(const std::string& type, std::size_t size, std::size_t align = 0) {
        std::lock_guard<std::mutex> lk(mutex_);
        expectations_[type] = {type, size, align};
    }

    Result validate(const std::string& type, std::size_t actualSize, std::size_t actualAlign = 0) {
        std::lock_guard<std::mutex> lk(mutex_);
        Result r;
        r.type = type;
        r.actualSize = actualSize;
        r.actualAlign = actualAlign;
        auto it = expectations_.find(type);
        if (it != expectations_.end()) {
            r.expectedSize = it->second.expectedSize;
            r.expectedAlign = it->second.expectedAlign;
            r.sizeOk = (actualSize == it->second.expectedSize);
            r.alignOk = (it->second.expectedAlign == 0) ||
                        (actualAlign == it->second.expectedAlign);
        }
        results_[type] = r;
        if (!r.sizeOk || !r.alignOk) ++failures_;
        return r;
    }

    std::vector<Result> failures() const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::vector<Result> out;
        for (const auto& [t, r] : results_)
            if (!r.sizeOk || !r.alignOk) out.push_back(r);
        return out;
    }

    std::size_t failureCount() const { return failures_.load(); }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        expectations_.clear();
        results_.clear();
        failures_ = 0;
    }

private:
    StructSizeValidator() = default;
    ~StructSizeValidator() = default;
    StructSizeValidator(const StructSizeValidator&) = delete;
    StructSizeValidator& operator=(const StructSizeValidator&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::unordered_map<std::string, Expectation> expectations_;
    std::unordered_map<std::string, Result> results_;
    std::atomic<std::size_t> failures_{0};
};

} // namespace nexus::utility::crossplatform
