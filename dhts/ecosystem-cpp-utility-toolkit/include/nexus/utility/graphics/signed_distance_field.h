#pragma once

#include <vector>
#include <optional>
#include <functional>
#include <cmath>
#include <limits>
#include <algorithm>
#include <nexus/utility/math/vector3_utils.h>
#include <nexus/utility/graphics/marching_cubes.h>

namespace nexus::utility::graphics {
using Vector3 = nexus::utility::math::Vector3;

/// @brief Signed distance field (SDF) sampled on a regular 3D grid.
///
/// An SDF stores, at each grid point, the signed distance to the nearest
/// surface: negative inside a solid, positive outside, zero on the surface.
/// This class builds SDFs from meshes or implicit functions, samples them with
/// trilinear interpolation, computes gradients, sphere-traces rays, and
/// extracts the zero (or arbitrary) isosurface as a triangle mesh.
class SignedDistanceField {
public:
    using Vector3 = nexus::utility::math::Vector3;

    /// @brief Regular scalar grid holding signed distances.
    ///
    /// Layout matches MarchingCubes::ScalarGrid so it can be passed directly to
    /// the isosurface extractor. Sample (i,j,k) lives at
    /// origin + (i,j,k)*cellSize and is stored at index
    /// i + nx*(j + ny*k).
    struct SDFGrid {
        Vector3 origin{0.0f, 0.0f, 0.0f};
        float cellSize = 1.0f;
        int nx = 0, ny = 0, nz = 0;
        std::vector<float> data;

        float at(int i, int j, int k) const {
            return data[index(i, j, k)];
        }
        float& at(int i, int j, int k) {
            return data[index(i, j, k)];
        }
        size_t index(int i, int j, int k) const {
            return static_cast<size_t>(i) +
                   static_cast<size_t>(nx) * (static_cast<size_t>(j) +
                   static_cast<size_t>(ny) * static_cast<size_t>(k));
        }
        Vector3 position(int i, int j, int k) const {
            return Vector3(origin.x + static_cast<float>(i) * cellSize,
                           origin.y + static_cast<float>(j) * cellSize,
                           origin.z + static_cast<float>(k) * cellSize);
        }
    };

    SignedDistanceField() = default;
    explicit SignedDistanceField(SDFGrid grid) : grid_(std::move(grid)) {}

    const SDFGrid& grid() const { return grid_; }
    SDFGrid& grid() { return grid_; }

    // ── Construction ────────────────────────────────────────────────────

    /// @brief Build an SDF from a closed triangle mesh.
    ///
    /// Unsigned distance is computed as the exact point-to-triangle distance to
    /// the nearest triangle. The sign is determined by an angle-weighted
    /// pseudonormal test: the vector from the closest surface point to the
    /// query point is compared against the pseudonormal at the closest feature.
    /// The grid is sized to the mesh bounds plus a padding margin.
    static SignedDistanceField fromMesh(const std::vector<Vector3>& vertices,
                                        const std::vector<unsigned>& indices,
                                        int resolution,
                                        float padding = -1.0f) {
        SDFGrid g;
        if (vertices.empty() || indices.size() < 3 || resolution < 2) {
            return SignedDistanceField(g);
        }

        Vector3 lo(std::numeric_limits<float>::max(),
                   std::numeric_limits<float>::max(),
                   std::numeric_limits<float>::max());
        Vector3 hi(std::numeric_limits<float>::lowest(),
                   std::numeric_limits<float>::lowest(),
                   std::numeric_limits<float>::lowest());
        for (const Vector3& v : vertices) {
            lo.x = std::min(lo.x, v.x); lo.y = std::min(lo.y, v.y); lo.z = std::min(lo.z, v.z);
            hi.x = std::max(hi.x, v.x); hi.y = std::max(hi.y, v.y); hi.z = std::max(hi.z, v.z);
        }
        Vector3 extent = hi - lo;
        float maxExtent = std::max({extent.x, extent.y, extent.z, 1e-6f});
        float pad = (padding >= 0.0f) ? padding : maxExtent * 0.1f;
        lo -= Vector3(pad, pad, pad);
        hi += Vector3(pad, pad, pad);

        extent = hi - lo;
        maxExtent = std::max({extent.x, extent.y, extent.z, 1e-6f});
        float cell = maxExtent / static_cast<float>(resolution - 1);

        g.origin = lo;
        g.cellSize = cell;
        g.nx = static_cast<int>(std::ceil(extent.x / cell)) + 1;
        g.ny = static_cast<int>(std::ceil(extent.y / cell)) + 1;
        g.nz = static_cast<int>(std::ceil(extent.z / cell)) + 1;
        g.nx = std::max(g.nx, 2);
        g.ny = std::max(g.ny, 2);
        g.nz = std::max(g.nz, 2);
        g.data.resize(static_cast<size_t>(g.nx) * g.ny * g.nz);

        const size_t triCount = indices.size() / 3;

        for (int k = 0; k < g.nz; ++k) {
            for (int j = 0; j < g.ny; ++j) {
                for (int i = 0; i < g.nx; ++i) {
                    const Vector3 p = g.position(i, j, k);
                    float bestDistSq = std::numeric_limits<float>::max();
                    Vector3 bestClosest;
                    Vector3 bestPseudoNormal;
                    for (size_t t = 0; t < triCount; ++t) {
                        const unsigned ia = indices[t * 3 + 0];
                        const unsigned ib = indices[t * 3 + 1];
                        const unsigned ic = indices[t * 3 + 2];
                        if (ia >= vertices.size() || ib >= vertices.size() ||
                            ic >= vertices.size()) continue;
                        const Vector3& a = vertices[ia];
                        const Vector3& b = vertices[ib];
                        const Vector3& c = vertices[ic];
                        Vector3 closest = closestPointOnTriangle(p, a, b, c);
                        Vector3 diff = p - closest;
                        float dSq = diff.lengthSquared();
                        if (dSq < bestDistSq) {
                            bestDistSq = dSq;
                            bestClosest = closest;
                            bestPseudoNormal = cross(b - a, c - a);
                        }
                    }
                    float dist = std::sqrt(bestDistSq);
                    Vector3 outward = p - bestClosest;
                    float sign = (dot(outward, bestPseudoNormal) < 0.0f) ? -1.0f : 1.0f;
                    g.at(i, j, k) = dist * sign;
                }
            }
        }
        return SignedDistanceField(std::move(g));
    }

    /// @brief Build an SDF by sampling an implicit function on a grid.
    ///
    /// @param fn     scalar field returning signed distance (or any implicit).
    /// @param boundsMin minimum corner of the sampled region.
    /// @param boundsMax maximum corner of the sampled region.
    /// @param resolution number of samples along the longest axis.
    static SignedDistanceField fromFunction(const std::function<float(const Vector3&)>& fn,
                                            const Vector3& boundsMin,
                                            const Vector3& boundsMax,
                                            int resolution) {
        SDFGrid g;
        if (resolution < 2) return SignedDistanceField(g);
        Vector3 extent = boundsMax - boundsMin;
        float maxExtent = std::max({extent.x, extent.y, extent.z, 1e-6f});
        float cell = maxExtent / static_cast<float>(resolution - 1);
        g.origin = boundsMin;
        g.cellSize = cell;
        g.nx = std::max(static_cast<int>(std::ceil(extent.x / cell)) + 1, 2);
        g.ny = std::max(static_cast<int>(std::ceil(extent.y / cell)) + 1, 2);
        g.nz = std::max(static_cast<int>(std::ceil(extent.z / cell)) + 1, 2);
        g.data.resize(static_cast<size_t>(g.nx) * g.ny * g.nz);
        for (int k = 0; k < g.nz; ++k)
            for (int j = 0; j < g.ny; ++j)
                for (int i = 0; i < g.nx; ++i)
                    g.at(i, j, k) = fn(g.position(i, j, k));
        return SignedDistanceField(std::move(g));
    }

    // ── Sampling ────────────────────────────────────────────────────────

    /// @brief Trilinearly interpolated signed distance at a world position.
    ///
    /// Positions outside the grid are clamped to the domain boundary.
    float evaluate(const Vector3& pos) const {
        if (grid_.nx < 2 || grid_.ny < 2 || grid_.nz < 2) return 0.0f;
        float gx = (pos.x - grid_.origin.x) / grid_.cellSize;
        float gy = (pos.y - grid_.origin.y) / grid_.cellSize;
        float gz = (pos.z - grid_.origin.z) / grid_.cellSize;

        gx = std::clamp(gx, 0.0f, static_cast<float>(grid_.nx - 1));
        gy = std::clamp(gy, 0.0f, static_cast<float>(grid_.ny - 1));
        gz = std::clamp(gz, 0.0f, static_cast<float>(grid_.nz - 1));

        int i0 = static_cast<int>(std::floor(gx));
        int j0 = static_cast<int>(std::floor(gy));
        int k0 = static_cast<int>(std::floor(gz));
        int i1 = std::min(i0 + 1, grid_.nx - 1);
        int j1 = std::min(j0 + 1, grid_.ny - 1);
        int k1 = std::min(k0 + 1, grid_.nz - 1);

        float tx = gx - static_cast<float>(i0);
        float ty = gy - static_cast<float>(j0);
        float tz = gz - static_cast<float>(k0);

        float c000 = grid_.at(i0, j0, k0);
        float c100 = grid_.at(i1, j0, k0);
        float c010 = grid_.at(i0, j1, k0);
        float c110 = grid_.at(i1, j1, k0);
        float c001 = grid_.at(i0, j0, k1);
        float c101 = grid_.at(i1, j0, k1);
        float c011 = grid_.at(i0, j1, k1);
        float c111 = grid_.at(i1, j1, k1);

        float c00 = c000 * (1 - tx) + c100 * tx;
        float c10 = c010 * (1 - tx) + c110 * tx;
        float c01 = c001 * (1 - tx) + c101 * tx;
        float c11 = c011 * (1 - tx) + c111 * tx;

        float c0 = c00 * (1 - ty) + c10 * ty;
        float c1 = c01 * (1 - ty) + c11 * ty;

        return c0 * (1 - tz) + c1 * tz;
    }

    /// @brief Gradient of the field via central differences (world space).
    Vector3 gradient(const Vector3& pos) const {
        const float h = grid_.cellSize * 0.5f;
        if (h < 1e-8f) return Vector3();
        float dx = evaluate(pos + Vector3(h, 0, 0)) - evaluate(pos - Vector3(h, 0, 0));
        float dy = evaluate(pos + Vector3(0, h, 0)) - evaluate(pos - Vector3(0, h, 0));
        float dz = evaluate(pos + Vector3(0, 0, h)) - evaluate(pos - Vector3(0, 0, h));
        return Vector3(dx, dy, dz) / (2.0f * h);
    }

    /// @brief Sphere-trace a ray against the isosurface.
    ///
    /// Steps along the ray by the current absolute distance value until it hits
    /// the surface (|dist| < epsilon) or exceeds maxDist. Returns the hit point
    /// if found. Works for rays starting outside the surface.
    std::optional<Vector3> rayMarch(const Vector3& origin, const Vector3& direction,
                                    float maxDist,
                                    float epsilon = 1e-4f, int maxSteps = 256) const {
        Vector3 dir = direction.normalized();
        if (dir.lengthSquared() < 1e-12f) return std::nullopt;
        float t = 0.0f;
        for (int step = 0; step < maxSteps && t < maxDist; ++step) {
            Vector3 p = origin + dir * t;
            float d = evaluate(p);
            if (std::abs(d) < epsilon) return p;
            // Advance by the distance to the surface; guard against zero steps.
            float advance = std::abs(d);
            if (advance < epsilon) advance = epsilon;
            t += advance;
        }
        return std::nullopt;
    }

    /// @brief Extract the isosurface as a triangle mesh via Marching Cubes.
    MarchingCubes::Mesh toMesh(float isoLevel = 0.0f) const {
        return MarchingCubes::extractSurface(grid_, isoLevel);
    }

    // ── Static implicit primitives and CSG operations ───────────────────

    static float sphereSDF(const Vector3& p, const Vector3& center, float radius) {
        return (p - center).length() - radius;
    }

    static float boxSDF(const Vector3& p, const Vector3& boxMin, const Vector3& boxMax) {
        const Vector3 c = (boxMin + boxMax) * 0.5f;
        const Vector3 halfExt = (boxMax - boxMin) * 0.5f;
        const Vector3 d(std::abs(p.x - c.x) - halfExt.x,
                        std::abs(p.y - c.y) - halfExt.y,
                        std::abs(p.z - c.z) - halfExt.z);
        const Vector3 dPos(std::max(d.x, 0.0f), std::max(d.y, 0.0f), std::max(d.z, 0.0f));
        const float outside = dPos.length();
        const float inside = std::min(std::max(d.x, std::max(d.y, d.z)), 0.0f);
        return outside + inside;
    }

    /// @brief SDF of an infinite-height capped cylinder along an arbitrary axis.
    ///
    /// The cylinder spans from base a to cap b with the given radius.
    static float cylinderSDF(const Vector3& p, const Vector3& a, const Vector3& b,
                             float radius) {
        const Vector3 ba = b - a;
        const Vector3 pa = p - a;
        const float baba = dot(ba, ba);
        if (baba < 1e-12f) return (p - a).length() - radius;
        const float paba = dot(pa, ba);
        // Radial distance from the axis.
        const Vector3 axisPoint = a + ba * (paba / baba);
        const float radial = (p - axisPoint).length() - radius;
        // Axial distance outside the caps [0, baba].
        const float halfLen = std::sqrt(baba) * 0.5f;
        const float mid = paba / std::sqrt(baba);
        const float axial = std::abs(mid - halfLen) - halfLen;
        const float dx = radial;
        const float dy = axial;
        const float outside = std::sqrt(std::max(dx, 0.0f) * std::max(dx, 0.0f) +
                                        std::max(dy, 0.0f) * std::max(dy, 0.0f));
        const float inside = std::min(std::max(dx, dy), 0.0f);
        return outside + inside;
    }

    static float unionSDF(float a, float b) { return std::min(a, b); }
    static float intersectionSDF(float a, float b) { return std::max(a, b); }
    static float differenceSDF(float a, float b) { return std::max(a, -b); }

    /// @brief Smooth polynomial union (blends the two fields over radius k).
    static float smoothUnionSDF(float a, float b, float k) {
        if (k <= 0.0f) return std::min(a, b);
        float h = std::clamp(0.5f + 0.5f * (b - a) / k, 0.0f, 1.0f);
        return b * (1.0f - h) + a * h - k * h * (1.0f - h);
    }

private:
    SDFGrid grid_;

    static float dot(const Vector3& a, const Vector3& b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }
    static Vector3 cross(const Vector3& a, const Vector3& b) {
        return {a.y * b.z - a.z * b.y,
                a.z * b.x - a.x * b.z,
                a.x * b.y - a.y * b.x};
    }

    // Exact closest point on a triangle to a query point (Ericson, RTCD).
    static Vector3 closestPointOnTriangle(const Vector3& p,
                                          const Vector3& a, const Vector3& b,
                                          const Vector3& c) {
        const Vector3 ab = b - a;
        const Vector3 ac = c - a;
        const Vector3 ap = p - a;
        float d1 = dot(ab, ap);
        float d2 = dot(ac, ap);
        if (d1 <= 0.0f && d2 <= 0.0f) return a;

        const Vector3 bp = p - b;
        float d3 = dot(ab, bp);
        float d4 = dot(ac, bp);
        if (d3 >= 0.0f && d4 <= d3) return b;

        float vc = d1 * d4 - d3 * d2;
        if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
            float v = d1 / (d1 - d3);
            return a + ab * v;
        }

        const Vector3 cp = p - c;
        float d5 = dot(ab, cp);
        float d6 = dot(ac, cp);
        if (d6 >= 0.0f && d5 <= d6) return c;

        float vb = d5 * d2 - d1 * d6;
        if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
            float w = d2 / (d2 - d6);
            return a + ac * w;
        }

        float va = d3 * d6 - d5 * d4;
        if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
            float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
            return b + (c - b) * w;
        }

        float denom = 1.0f / (va + vb + vc);
        float v = vb * denom;
        float w = vc * denom;
        return a + ab * v + ac * w;
    }
};

} // namespace nexus::utility::graphics
