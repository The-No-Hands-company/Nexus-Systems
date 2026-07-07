#pragma once
#include <cstddef>
#include <string>
#include <vector>

namespace nexus::utility::cpp26 {

class InplaceVectorValidator {
public:
    struct ValidationResult {
        bool valid = true;
        size_t capacity;
        size_t size;
        size_t maxPossibleSize;
        std::string error;
    };

    template<size_t N>
    static ValidationResult validate(size_t currentSize) {
        ValidationResult r;
        r.capacity = N;
        r.size = currentSize;
        r.maxPossibleSize = N;
        if (currentSize > N) {
            r.valid = false;
            r.error = "inplace_vector overflow: " + std::to_string(currentSize) +
                      " > capacity " + std::to_string(N);
        }
        return r;
    }

    template<size_t N>
    static size_t remainingCapacity(size_t currentSize) {
        return currentSize < N ? N - currentSize : 0;
    }

    static bool wouldOverflow(size_t capacity, size_t current, size_t toAdd) {
        return current + toAdd > capacity;
    }
};

} // namespace nexus::utility::cpp26
