#pragma once
#include <cstddef>
#include <vector>
#include <string>
#include <sstream>
#include <cmath>

namespace nexus::utility::cpp26 {

class LinalgValidator {
public:
    struct MatrixShape {
        size_t rows;
        size_t cols;
        size_t elementSize;
        bool isContiguous;
    };

    struct Validation {
        bool valid = true;
        std::string error;
        double conditionNumber = 1.0;
        bool isSymmetric = false;
        bool isPositiveDefinite = false;
    };

    bool checkDimensions(size_t rowsA, size_t colsA, size_t rowsB, size_t colsB) const {
        if (colsA != rowsB) {
            lastError_ = "Dimension mismatch: " + std::to_string(colsA) +
                         " != " + std::to_string(rowsB);
            return false;
        }
        return true;
    }

    std::string getLastError() const { return lastError_; }

    static double estimateConditionNumber(const std::vector<double>& matrix, size_t n) {
        double norm = 0.0;
        for (size_t i = 0; i < n && i * n < matrix.size(); i++) {
            double rowSum = 0.0;
            for (size_t j = 0; j < n; j++) rowSum += std::abs(matrix[i * n + j]);
            norm = std::max(norm, rowSum);
        }
        return norm; // Simplified infinity-norm approximation
    }

    void clear() { lastError_.clear(); }

private:
    mutable std::string lastError_;
};

} // namespace nexus::utility::cpp26
