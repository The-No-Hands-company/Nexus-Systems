#pragma once

#include <string>
#include <vector>
#include <utility>
#include <atomic>
#include <mutex>
#include <cstddef>

namespace nexus::utility::algorithmic {

/**
 * @brief Validate sorting algorithm correctness (ordering and stability).
 */
class SortingValidator {
public:
    static SortingValidator& instance() {
        static SortingValidator inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    void enable() { enabled_ = true; }
    void disable() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    /// Validate that @p data is in non-decreasing (ascending) order.
    bool validateSorted(const std::vector<int>& data) {
        bool ok = true;
        for (std::size_t i = 1; i < data.size(); ++i) {
            if (data[i - 1] > data[i]) { ok = false; break; }
        }
        std::lock_guard<std::mutex> lk(mutex_);
        ++validationCount_;
        if (!ok) ++failureCount_;
        return ok;
    }

    /// Validate stability: equal keys must preserve their relative input order.
    bool validateStable(const std::vector<std::pair<int, std::string>>& before,
                        const std::vector<std::pair<int, std::string>>& after) {
        bool ok = (before.size() == after.size());
        if (ok) {
            // For each key value, the sequence of payloads in `after` must match
            // the order in which they appeared in `before`.
            for (std::size_t i = 1; i < after.size() && ok; ++i) {
                if (after[i - 1].first > after[i].first) { ok = false; break; }
            }
            for (std::size_t i = 0; i < after.size() && ok; ++i) {
                // Find i-th occurrence of after[i].first among equal keys and
                // ensure payload order matches the stable input order.
                int key = after[i].first;
                std::size_t rank = 0;
                for (std::size_t j = 0; j <= i; ++j) {
                    if (after[j].first == key) ++rank;
                }
                std::size_t seen = 0;
                bool matched = false;
                for (const auto& b : before) {
                    if (b.first == key) {
                        ++seen;
                        if (seen == rank) {
                            matched = (b.second == after[i].second);
                            break;
                        }
                    }
                }
                if (!matched) ok = false;
            }
        }
        std::lock_guard<std::mutex> lk(mutex_);
        ++validationCount_;
        if (!ok) ++failureCount_;
        return ok;
    }

    std::size_t getValidationCount() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return validationCount_;
    }
    std::size_t getFailureCount() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return failureCount_;
    }

    std::string getStatus() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return "SortingValidator{validations=" + std::to_string(validationCount_) +
               ", failures=" + std::to_string(failureCount_) + "}";
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        validationCount_ = 0;
        failureCount_ = 0;
    }

private:
    SortingValidator() = default;
    ~SortingValidator() = default;
    SortingValidator(const SortingValidator&) = delete;
    SortingValidator& operator=(const SortingValidator&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::size_t validationCount_ = 0;
    std::size_t failureCount_ = 0;
};

} // namespace nexus::utility::algorithmic
