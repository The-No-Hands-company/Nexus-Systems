#pragma once

#include <vector>
#include <cmath>
#include <string>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <numeric>

namespace nexus::utility::vision {

/// Validates image filter operations for correctness and artifacts
class FilterValidator {
public:
    struct FilterKernel {
        std::vector<double> weights;
        int width = 0, height = 0;
        double sum() const { return std::accumulate(weights.begin(), weights.end(), 0.0); }
        bool isNormalized() const { return std::abs(sum() - 1.0) < 1e-6 || std::abs(sum()) < 1e-6; }
        bool isSeparable() const;
    };

    struct ValidationResult {
        bool valid = true;
        bool is_symmetric = false;
        bool is_separable = false;
        bool is_normalized = false;
        double frequency_response_sum = 0;
        double dc_gain = 0;
        std::vector<std::string> warnings;
    };

    static ValidationResult validate(const FilterKernel& kernel) {
        ValidationResult r;
        if (kernel.weights.empty()) { r.valid = false; r.warnings.push_back("Empty kernel"); return r; }

        r.is_normalized = kernel.isNormalized();
        if (!r.is_normalized) r.warnings.push_back("Kernel not normalized");

        r.is_symmetric = checkSymmetry(kernel);
        r.is_separable = kernel.isSeparable();

        auto freq = frequencyResponse(kernel);
        r.frequency_response_sum = freq;
        r.dc_gain = computeDCGain(kernel);

        if (r.dc_gain < 0.9 || r.dc_gain > 1.1)
            r.warnings.push_back("DC gain out of [0.9,1.1]: signal amplification/attenuation");

        return r;
    }

    /// Generate common filter kernels
    static FilterKernel gaussian(int size, double sigma) {
        FilterKernel k;
        k.width = k.height = size;
        int half = size / 2;
        for (int y = -half; y <= half; ++y)
            for (int x = -half; x <= half; ++x)
                k.weights.push_back(std::exp(-(x*x + y*y) / (2*sigma*sigma)) / (2*M_PI*sigma*sigma));
        double s = k.sum();
        for (auto& w : k.weights) w /= s;
        return k;
    }

    static FilterKernel sobelX() {
        FilterKernel k; k.width = k.height = 3;
        k.weights = {-1,0,1, -2,0,2, -1,0,1};
        return k;
    }

    static FilterKernel sobelY() {
        FilterKernel k; k.width = k.height = 3;
        k.weights = {-1,-2,-1, 0,0,0, 1,2,1};
        return k;
    }

    static FilterKernel laplacian() {
        FilterKernel k; k.width = k.height = 3;
        k.weights = {0,1,0, 1,-4,1, 0,1,0};
        return k;
    }

    static FilterKernel boxBlur(int size) {
        FilterKernel k; k.width = k.height = size;
        double val = 1.0 / (size * size);
        k.weights.resize(size*size, val);
        return k;
    }

    static std::string report(const ValidationResult& r) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(4);
        oss << "═══ Filter Validation ═══\n";
        oss << "  Symmetric:   " << (r.is_symmetric ? "✓ yes" : "⚠ no") << "\n";
        oss << "  Separable:   " << (r.is_separable ? "✓ yes" : "- no") << "\n";
        oss << "  Normalized:  " << (r.is_normalized ? "✓ yes" : "⚠ no") << "\n";
        oss << "  DC Gain:     " << r.dc_gain << "\n";
        for (auto& w : r.warnings) oss << "  ⚠ " << w << "\n";
        return oss.str();
    }

private:
    static bool checkSymmetry(const FilterKernel& k) {
        int hw = k.width / 2, hh = k.height / 2;
        for (int y = 0; y <= hh; ++y)
            for (int x = 0; x <= hw; ++x)
                if (std::abs(k.weights[y*k.width+x] - k.weights[(k.height-1-y)*k.width+(k.width-1-x)]) > 1e-8)
                    return false;
        return true;
    }

    static double frequencyResponse(const FilterKernel& k) {
        double sum = 0;
        for (int f = 0; f < 64; ++f)
            for (auto& w : k.weights) sum += std::abs(w);
        return sum / 64;
    }

    static double computeDCGain(const FilterKernel& k) { return k.sum(); }
};

inline bool FilterValidator::FilterKernel::isSeparable() const {
    if (width != height || width < 3) return false;
    // SVD-like check: 3x3 separable if rank 1
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
            if (std::abs(weights[y*width+x] * weights[0] - weights[y*width] * weights[x]) > 1e-8)
                return false;
    return true;
}

} // namespace nexus::utility::vision
