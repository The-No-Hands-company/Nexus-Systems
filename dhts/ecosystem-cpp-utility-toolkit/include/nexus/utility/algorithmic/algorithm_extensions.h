#pragma once

#include <algorithm>
#include <iterator>
#include <random>
#include <vector>

namespace nexus::utility::algorithmic {

class AlgorithmExtensions {
public:
    template<typename Container>
    static bool isSorted(const Container& container) {
        return std::is_sorted(std::begin(container), std::end(container));
    }

    template<typename T>
    static bool binarySearch(const std::vector<T>& vec, const T& value) {
        return std::binary_search(vec.begin(), vec.end(), value);
    }

    template<typename T>
    static auto lowerBound(const std::vector<T>& vec, const T& value) {
        return std::lower_bound(vec.begin(), vec.end(), value);
    }

    template<typename T>
    static auto upperBound(const std::vector<T>& vec, const T& value) {
        return std::upper_bound(vec.begin(), vec.end(), value);
    }

    template<typename Container>
    static void shuffle(Container& container) {
        static thread_local std::mt19937 rng(std::random_device{}());
        std::shuffle(std::begin(container), std::end(container), rng);
    }
};

} // namespace nexus::utility::algorithmic
