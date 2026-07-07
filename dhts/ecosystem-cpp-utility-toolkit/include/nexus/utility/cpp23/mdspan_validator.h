#pragma once

#include <cstddef>
#include <vector>
#include <mutex>

namespace nexus::utility::cpp23 {

/// @brief Validate std::mdspan dimensions and element access bounds.
class MdspanValidator {
public:
    static MdspanValidator& instance() {
        static MdspanValidator inst;
        return inst;
    }

    void initialize() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = true;
    }

    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
        violations_ = 0;
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    /// Returns true if dims exactly matches expected. Counts a violation otherwise.
    bool validateDimensions(const std::vector<size_t>& dims,
                            const std::vector<size_t>& expected) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (dims.size() != expected.size()) {
            ++violations_;
            return false;
        }
        for (size_t i = 0; i < dims.size(); ++i) {
            if (dims[i] != expected[i]) {
                ++violations_;
                return false;
            }
        }
        return true;
    }

    /// Bounds check: indices must match rank and each index < corresponding extent.
    bool validateAccess(const std::vector<size_t>& indices,
                        const std::vector<size_t>& extents) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (indices.size() != extents.size()) {
            ++violations_;
            return false;
        }
        for (size_t i = 0; i < indices.size(); ++i) {
            if (indices[i] >= extents[i]) {
                ++violations_;
                return false;
            }
        }
        return true;
    }

    size_t getViolationCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return violations_;
    }

private:
    MdspanValidator() = default;
    ~MdspanValidator() = default;
    MdspanValidator(const MdspanValidator&) = delete;
    MdspanValidator& operator=(const MdspanValidator&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    size_t violations_ = 0;
};

} // namespace nexus::utility::cpp23
