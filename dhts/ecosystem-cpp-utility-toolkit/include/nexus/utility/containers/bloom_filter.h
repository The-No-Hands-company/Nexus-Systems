#pragma once

#include <vector>
#include <functional>
#include <cstdint>
#include <cmath>

namespace nexus::utility::containers {

/**
 * @brief Bloom filter for probabilistic membership testing.
 */
template<typename T>
class BloomFilter {
public:
    /**
     * @brief Constructs a bloom filter.
     * @param expected_elements Expected number of elements.
     * @param false_positive_rate Desired false positive rate (0.0 - 1.0).
     */
    explicit BloomFilter(size_t expected_elements, double false_positive_rate = 0.01) {
        // Calculate optimal size and hash count
        size_ = static_cast<size_t>(
            -static_cast<double>(expected_elements) * std::log(false_positive_rate) / 
            (std::log(2.0) * std::log(2.0))
        );
        
        hash_count_ = static_cast<size_t>(
            (static_cast<double>(size_) / expected_elements) * std::log(2.0)
        );
        
        if (hash_count_ == 0) hash_count_ = 1;
        
        bits_.resize(size_, false);
    }

    /**
     * @brief Inserts an element.
     */
    void insert(const T& element) {
        for (size_t i = 0; i < hash_count_; ++i) {
            size_t hash = computeHash(element, i);
            bits_[hash % size_] = true;
        }
    }

    /**
     * @brief Checks if element might be in the set.
     * @return true if possibly present (may have false positives), false if definitely not present.
     */
    [[nodiscard]] bool contains(const T& element) const {
        for (size_t i = 0; i < hash_count_; ++i) {
            size_t hash = computeHash(element, i);
            if (!bits_[hash % size_]) {
                return false;
            }
        }
        return true;
    }

    void clear() noexcept {
        std::fill(bits_.begin(), bits_.end(), false);
    }

    [[nodiscard]] size_t size() const noexcept { return size_; }
    [[nodiscard]] size_t hashCount() const noexcept { return hash_count_; }

private:
    std::vector<bool> bits_;
    size_t size_;
    size_t hash_count_;

    size_t computeHash(const T& element, size_t seed) const noexcept {
        std::hash<T> hasher;
        size_t hash = hasher(element);
        // Simple hash combination with seed
        return hash ^ (seed * 0x9e3779b9 + (hash << 6) + (hash >> 2));
    }
};

} // namespace nexus::utility::containers
