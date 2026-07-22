#include <nexus/geometry/CurveFairing.h>
#include <nexus/geometry/Tolerance.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

namespace {

// Compute the 3D bounding box of control points for scale-adaptive tolerances
static void computeControlPointBounds(const NurbsCurve& curve, 
                                    float& minX, float& minY, float& minZ,
                                    float& maxX, float& maxY, float& maxZ) {
    const auto& ctrlPts = curve.controlPoints();
    if (ctrlPts.empty()) return;

    minX = maxX = ctrlPts[0].x;
    minY = maxY = ctrlPts[0].y;
    minZ = maxZ = ctrlPts[0].z;

    for (size_t i = 1; i < ctrlPts.size(); ++i) {
        const Vec3& p = ctrlPts[i];
        minX = std::min(minX, p.x);
        maxX = std::max(maxX, p.x);
        minY = std::min(minY, p.y);
        maxY = std::max(maxY, p.y);
        minZ = std::min(minZ, p.z);
        maxZ = std::max(maxZ, p.z);
    }
}

// Compute scale-adaptive tolerance for pivot checks in linear solving
static float computePivotTolerance(const NurbsCurve& curve) {
    // Zero-init to satisfy GCC-15 -Werror=maybe-uninitialized (computeControlPointBounds
    // fills these by reference, but the compiler cannot prove the empty-input path does).
    float minX = 0.f, minY = 0.f, minZ = 0.f, maxX = 0.f, maxY = 0.f, maxZ = 0.f;
    computeControlPointBounds(curve, minX, minY, minZ, maxX, maxY, maxZ);
    
    // Compute the diagonal length of the bounding box as a scale measure
    float dx = maxX - minX;
    float dy = maxY - minY;
    float dz = maxZ - minZ;
    float maxDim = std::max({dx, dy, dz});
    return std::max(maxDim, 1.0f); // Ensure minimum scale of 1.0 to avoid division by zero
}

static void solveSystem(std::vector<float>& A, std::vector<float>& b, int32_t n, float pivotTolerance)
{
    auto at = [&](int32_t r, int32_t c) -> float& {
        return A[static_cast<size_t>(r) * static_cast<size_t>(n) + static_cast<size_t>(c)];
    };

    for (int32_t col = 0; col < n; ++col) {
        int32_t maxRow = col;
        float maxVal = std::abs(at(col, col));
        for (int32_t row = col + 1; row < n; ++row) {
            float val = std::abs(at(row, col));
            if (val > maxVal) {
                maxVal = val;
                maxRow = row;
            }
        }
        if (maxRow != col) {
            for (int32_t j = col; j < n; ++j) {
                std::swap(at(col, j), at(maxRow, j));
            }
            std::swap(b[static_cast<size_t>(col)], b[static_cast<size_t>(maxRow)]);
        }
        float pivot = at(col, col);
        if (std::abs(pivot) < pivotTolerance) continue;
        for (int32_t row = col + 1; row < n; ++row) {
            float factor = at(row, col) / pivot;
            if (factor == 0.f) continue;
            for (int32_t j = col; j < n; ++j) {
                at(row, j) -= factor * at(col, j);
            }
            b[static_cast<size_t>(row)] -= factor * b[static_cast<size_t>(col)];
        }
    }
    for (int32_t col = n - 1; col >= 0; --col) {
        float sum = b[static_cast<size_t>(col)];
        for (int32_t j = col + 1; j < n; ++j) {
            sum -= at(col, j) * b[static_cast<size_t>(j)];
        }
        b[static_cast<size_t>(col)] = sum / at(col, col);
    }
}

static void buildSystem(std::vector<float>& A, std::vector<float>& b,
                         const float* oldCoords,
                         float lambda, bool fixEndpoints, int32_t n)
{
    size_t nn = static_cast<size_t>(n) * static_cast<size_t>(n);
    A.assign(nn, 0.f);
    b.assign(oldCoords, oldCoords + n);

    auto at = [&](int32_t r, int32_t c) -> float& {
        return A[static_cast<size_t>(r) * static_cast<size_t>(n) + static_cast<size_t>(c)];
    };

    for (int32_t i = 0; i < n; ++i) {
        float ltl_a = 0.f;
        float ltl_b = 0.f;
        float ltl_c = 0.f;
        float ltl_d = 0.f;
        float ltl_e = 0.f;

        if (i >= 2)           ltl_a = 1.f;
        if (i == 1 || i == n - 1) ltl_b = -2.f;
        else if (i >= 2 && i <= n - 2) ltl_b = -4.f;
        if (i == 0 || i == n - 1)      ltl_c = 1.f;
        else if (i == 1 || i == n - 2) ltl_c = 5.f;
        else                           ltl_c = 6.f;
        if (i == n - 2)                ltl_d = -2.f;
        else if (i >= 0 && i <= n - 3) ltl_d = -4.f;
        if (i >= 0 && i <= n - 3)      ltl_e = 1.f;

        if (i - 2 >= 0) at(i, i - 2) = lambda * ltl_a;
        if (i - 1 >= 0) at(i, i - 1) = lambda * ltl_b;
        at(i, i) = 1.f + lambda * ltl_c;
        if (i + 1 < n)  at(i, i + 1) = lambda * ltl_d;
        if (i + 2 < n)  at(i, i + 2) = lambda * ltl_e;
    }

    if (fixEndpoints) {
        for (int32_t j = 0; j < n; ++j) {
            at(0, j) = (j == 0) ? 1.f : 0.f;
            at(n - 1, j) = (j == n - 1) ? 1.f : 0.f;
        }
    }
}

} // anonymous namespace

NurbsCurve CurveFairing::fair(const NurbsCurve& curve,
                               const CurveFairingOptions& opts) {
    if (!curve.isValid()) return curve;

    auto ctl = curve.controlPoints();
    int32_t n = static_cast<int32_t>(ctl.size());
    if (n < 4) return curve;

    float lambda = std::clamp(opts.strength, 0.f, 1.f);
    if (lambda >= 1.f) lambda = 0.999f;
    lambda = lambda / (1.f - lambda);

    const auto& origCtl = curve.controlPoints();
    std::vector<Vec3> current = ctl;

    std::vector<float> oldChannel(static_cast<size_t>(n));
    std::vector<float> A;
    std::vector<float> b;

    for (int32_t iter = 0; iter < opts.iterations; ++iter) {
        for (int coord = 0; coord < 3; ++coord) {
            for (int32_t i = 0; i < n; ++i) {
                size_t si = static_cast<size_t>(i);
                if (coord == 0)      oldChannel[si] = current[si].x;
                else if (coord == 1) oldChannel[si] = current[si].y;
                else                 oldChannel[si] = current[si].z;

                if (opts.fixEndpoints && (i == 0 || i == n - 1)) {
                    oldChannel[si] = (coord == 0) ? origCtl[si].x
                                      : (coord == 1) ? origCtl[si].y
                                      : origCtl[si].z;
                }
            }

            buildSystem(A, b, oldChannel.data(), lambda, opts.fixEndpoints, n);
            solveSystem(A, b, n, 1e-12f * computePivotTolerance(curve));

            for (int32_t i = 0; i < n; ++i) {
                size_t si = static_cast<size_t>(i);
                if (coord == 0)      current[si].x = b[si];
                else if (coord == 1) current[si].y = b[si];
                else                 current[si].z = b[si];
            }
        }
    }

    NurbsCurve result = curve;
    result.setControlPoints(current);
    return result;
}

} // namespace nexus::geometry
