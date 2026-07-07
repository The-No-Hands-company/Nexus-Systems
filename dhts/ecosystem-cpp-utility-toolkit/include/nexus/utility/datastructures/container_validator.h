#pragma once

#include <cstddef>
#include <map>
#include <mutex>
#include <string>

namespace nexus::utility::datastructures {

/// @brief Validates container integrity: size/capacity consistency and iterator validity.
class ContainerValidator {
public:
    static ContainerValidator& instance() {
        static ContainerValidator inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = true;
        config_ = config;
    }

    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
        container_violations_.clear();
        total_violations_ = 0;
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    /// @brief Validate that a reported size matches the actual element count.
    /// @return true if consistent, false (and records a violation) otherwise.
    bool validateSize(size_t reported, size_t actual) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (reported != actual) {
            ++total_violations_;
            ++container_violations_["<size>"];
            return false;
        }
        return true;
    }

    /// @brief Validate that used never exceeds capacity.
    /// @return true if used <= capacity, false (and records a violation) otherwise.
    bool validateCapacity(size_t used, size_t capacity) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (used > capacity) {
            ++total_violations_;
            ++container_violations_["<capacity>"];
            return false;
        }
        return true;
    }

    /// @brief Record the validity of an iterator against a named container.
    /// @return true if the iterator is valid, false (and records a violation) otherwise.
    bool checkIteratorValidity(const std::string& container, bool valid) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!valid) {
            ++total_violations_;
            ++container_violations_[container];
            return false;
        }
        return true;
    }

    size_t getViolationCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return total_violations_;
    }

    size_t getContainerViolations(const std::string& container) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = container_violations_.find(container);
        return it != container_violations_.end() ? it->second : 0;
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        container_violations_.clear();
        total_violations_ = 0;
    }

private:
    ContainerValidator() = default;
    ~ContainerValidator() = default;
    ContainerValidator(const ContainerValidator&) = delete;
    ContainerValidator& operator=(const ContainerValidator&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::map<std::string, size_t> container_violations_;
    size_t total_violations_ = 0;
};

} // namespace nexus::utility::datastructures
