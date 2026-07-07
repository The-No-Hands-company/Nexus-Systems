#pragma once

#include <vector>
#include <stdexcept>
#include <limits>
#include <cstddef>

namespace nexus::utility::containers {

/**
 * @brief Sparse set for efficient storage of sparse integer sets.
 * Provides O(1) insert, remove, and contains operations.
 */
class SparseSet {
public:
    explicit SparseSet(size_t max_value)
        : sparse_(max_value == std::numeric_limits<size_t>::max()
                      ? throw std::overflow_error("SparseSet: max_value too large")
                      : max_value + 1, -1)
        , max_value_(max_value) {}

    /**
     * @brief Inserts a value into the set.
     */
    void insert(size_t value) {
        if (value > max_value_) {
            throw std::out_of_range("Value exceeds max_value");
        }
        
        if (contains(value)) {
            return; // Already present
        }
        
        sparse_[value] = static_cast<int>(dense_.size());
        dense_.push_back(value);
    }

    /**
     * @brief Removes a value from the set.
     */
    void remove(size_t value) {
        if (value > max_value_ || !contains(value)) {
            return;
        }

        int dense_index = sparse_[value];
        size_t last_value = dense_.back();

        if (value != last_value) {
            dense_[static_cast<size_t>(dense_index)] = last_value;
            sparse_[last_value] = dense_index;
        }

        dense_.pop_back();
        sparse_[value] = -1;
    }

    /**
     * @brief Checks if value is in the set.
     */
    bool contains(size_t value) const {
        if (value > max_value_) {
            return false;
        }
        
        int dense_index = sparse_[value];
        return dense_index >= 0 && 
               dense_index < static_cast<int>(dense_.size()) && 
               dense_[dense_index] == value;
    }

    size_t size() const { return dense_.size(); }
    bool empty() const { return dense_.empty(); }

    void clear() {
        dense_.clear();
        std::fill(sparse_.begin(), sparse_.end(), -1);
    }

    /**
     * @brief Iterates over all values in the set.
     */
    const std::vector<size_t>& values() const { return dense_; }

private:
    std::vector<int> sparse_;     // Maps value -> dense index (-1 if not present)
    std::vector<size_t> dense_;   // Stores actual values
    size_t max_value_;
};

} // namespace nexus::utility::containers
