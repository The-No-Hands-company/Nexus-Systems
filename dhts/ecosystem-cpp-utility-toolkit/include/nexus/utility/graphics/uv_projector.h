#pragma once

#include <vector>
#include <array>
#include <cstdint>
#include <cmath>
#include <limits>
#include <algorithm>
#include <nexus/utility/math/vector3_utils.h>

namespace nexus::utility::graphics {
using Vector3 = nexus::utility::math::Vector3;

/// @brief Texture-coordinate generation and atlas packing for meshes.
///
/// Offers the standard analytic projections (planar, cylindrical, spherical and
/// cube-map/box), a shelf/bin packer that arranges UV islands into a square
/// atlas by descending height, and a texel-density estimator relating UV area to
/// world area for a chosen texture resolution.
class UVProjector {
public:
    using Vector3 = nexus::utility::math::Vector3;

    struct UV {
        float u = 0.0f;
        float v = 0.0f;
    };

    struct Island {
        std::vector<unsigned> triangleIndices;
        std::vector<UV> uvs;
    };

    struct AtlasResult {
        std::vector<float> u;
        std::vector<float> v;
        int atlasWidth = 0;
        int atlasHeight = 0;
        float utilization = 0.0f;
    };

    // ── Analytic projections ────────────────────────────────────────────

    /// @brief Planar projection onto the plane spanned by uAxis/vAxis at origin.
    static std::vector<UV> planarProject(const std::vector<Vector3>& verts,
                                         const Vector3& origin,
                                         const Vector3& uAxis,
                                         const Vector3& vAxis) {
        Vector3 u = uAxis.normalized();
        Vector3 v = vAxis.normalized();
        std::vector<UV> out(verts.size());
        for (size_t i = 0; i < verts.size(); ++i) {
            Vector3 d = verts[i] - origin;
            out[i] = {dot(d, u), dot(d, v)};
        }
        return out;
    }

    /// @brief Cylindrical projection around `axis` through `center`.
    static std::vector<UV> cylindricalProject(const std::vector<Vector3>& verts,
                                              const Vector3& center,
                                              const Vector3& axis,
                                              float radius) {
        Vector3 a = axis.normalized();
        Vector3 ref = (std::abs(a.x) < 0.9f) ? Vector3(1, 0, 0) : Vector3(0, 1, 0);
        Vector3 e0 = cross(a, ref).normalized();
        Vector3 e1 = cross(a, e0).normalized();
        float denom = (radius > 1e-6f) ? (2.0f * PI * radius) : 1.0f;

        std::vector<UV> out(verts.size());
        for (size_t i = 0; i < verts.size(); ++i) {
            Vector3 d = verts[i] - center;
            float x = dot(d, e0);
            float y = dot(d, e1);
            float h = dot(d, a);
            float angle = std::atan2(y, x);
            out[i] = {0.5f + angle / (2.0f * PI), h / denom};
        }
        return out;
    }

    /// @brief Spherical (equirectangular) projection about `center`.
    static std::vector<UV> sphericalProject(const std::vector<Vector3>& verts,
                                            const Vector3& center) {
        std::vector<UV> out(verts.size());
        for (size_t i = 0; i < verts.size(); ++i) {
            Vector3 d = (verts[i] - center).normalized();
            float u = 0.5f + std::atan2(d.z, d.x) / (2.0f * PI);
            float v = 0.5f - std::asin(std::clamp(d.y, -1.0f, 1.0f)) / PI;
            out[i] = {u, v};
        }
        return out;
    }

    /// @brief Cube-map (box) projection using the dominant axis per vertex.
    static std::vector<UV> boxProject(const std::vector<Vector3>& verts,
                                      const Vector3& bboxMin,
                                      const Vector3& bboxMax) {
        Vector3 center = (bboxMin + bboxMax) * 0.5f;
        Vector3 size = bboxMax - bboxMin;
        float sx = (std::abs(size.x) > 1e-6f) ? size.x : 1.0f;
        float sy = (std::abs(size.y) > 1e-6f) ? size.y : 1.0f;
        float sz = (std::abs(size.z) > 1e-6f) ? size.z : 1.0f;

        std::vector<UV> out(verts.size());
        for (size_t i = 0; i < verts.size(); ++i) {
            Vector3 d = verts[i] - center;
            float ax = std::abs(d.x), ay = std::abs(d.y), az = std::abs(d.z);
            float u, v;
            if (ax >= ay && ax >= az) {
                u = (d.x >= 0.0f ? -d.z : d.z) / sz + 0.5f;
                v = d.y / sy + 0.5f;
            } else if (ay >= ax && ay >= az) {
                u = d.x / sx + 0.5f;
                v = (d.y >= 0.0f ? -d.z : d.z) / sz + 0.5f;
            } else {
                u = (d.z >= 0.0f ? d.x : -d.x) / sx + 0.5f;
                v = d.y / sy + 0.5f;
            }
            out[i] = {u, v};
        }
        return out;
    }

    // ── Atlas packing ───────────────────────────────────────────────────

    /// @brief Pack UV islands into a square atlas using a shelf algorithm.
    ///
    /// Islands are sorted by descending bounding-box height and placed left to
    /// right on shelves; the whole layout is finally scaled to fit [0,1]^2. The
    /// repacked UVs are returned island-by-island in the input order.
    static AtlasResult packAtlas(const std::vector<Island>& islands, int atlasSize) {
        AtlasResult result;
        result.atlasWidth = atlasSize;
        result.atlasHeight = atlasSize;
        if (islands.empty()) return result;

        struct Box {
            int island;
            float minU, minV, w, h;
        };
        std::vector<Box> boxes;
        boxes.reserve(islands.size());
        double totalArea = 0.0;
        for (size_t i = 0; i < islands.size(); ++i) {
            const auto& uvs = islands[i].uvs;
            if (uvs.empty()) { boxes.push_back({static_cast<int>(i), 0, 0, 0, 0}); continue; }
            float mnU = uvs[0].u, mxU = uvs[0].u, mnV = uvs[0].v, mxV = uvs[0].v;
            for (const auto& p : uvs) {
                mnU = std::min(mnU, p.u); mxU = std::max(mxU, p.u);
                mnV = std::min(mnV, p.v); mxV = std::max(mxV, p.v);
            }
            Box bx{static_cast<int>(i), mnU, mnV, mxU - mnU, mxV - mnV};
            totalArea += static_cast<double>(bx.w) * bx.h;
            boxes.push_back(bx);
        }

        // Shelf width chosen from the total area with slack.
        float shelfWidth = static_cast<float>(std::sqrt(totalArea) * 1.1);
        float maxBoxW = 0.0f;
        for (const auto& b : boxes) maxBoxW = std::max(maxBoxW, b.w);
        shelfWidth = std::max(shelfWidth, maxBoxW);
        if (shelfWidth < 1e-6f) shelfWidth = 1.0f;

        std::vector<int> order(boxes.size());
        for (size_t i = 0; i < order.size(); ++i) order[i] = static_cast<int>(i);
        std::sort(order.begin(), order.end(),
                  [&](int a, int b) { return boxes[a].h > boxes[b].h; });

        std::vector<float> placeX(boxes.size(), 0.0f), placeY(boxes.size(), 0.0f);
        float cursorX = 0.0f, cursorY = 0.0f, shelfH = 0.0f, usedHeight = 0.0f;
        for (int oi : order) {
            const Box& b = boxes[oi];
            if (cursorX + b.w > shelfWidth && cursorX > 0.0f) {
                cursorX = 0.0f;
                cursorY += shelfH;
                shelfH = 0.0f;
            }
            placeX[oi] = cursorX;
            placeY[oi] = cursorY;
            cursorX += b.w;
            shelfH = std::max(shelfH, b.h);
            usedHeight = std::max(usedHeight, cursorY + b.h);
        }

        float extent = std::max(shelfWidth, usedHeight);
        if (extent < 1e-6f) extent = 1.0f;
        float scale = 1.0f / extent;

        // Rewrite UVs island-by-island, offset into the packed layout.
        for (size_t i = 0; i < islands.size(); ++i) {
            const Box& b = boxes[i];
            for (const auto& p : islands[i].uvs) {
                float nu = (p.u - b.minU + placeX[i]) * scale;
                float nv = (p.v - b.minV + placeY[i]) * scale;
                result.u.push_back(nu);
                result.v.push_back(nv);
            }
        }

        float packedW = shelfWidth * scale;
        float packedH = usedHeight * scale;
        float denom = packedW * packedH;
        result.utilization = (denom > 1e-9f)
                                 ? static_cast<float>(totalArea * scale * scale / denom)
                                 : 0.0f;
        result.utilization = std::clamp(result.utilization, 0.0f, 1.0f);
        return result;
    }

    // ── Texel density ───────────────────────────────────────────────────

    /// @brief Average texels per world unit for the given UVs and texture size.
    static float computeTexelDensity(const std::vector<Vector3>& verts,
                                     const std::vector<unsigned>& indices,
                                     const std::vector<UV>& uvs,
                                     int textureSize) {
        double weightedSum = 0.0;
        double totalWorld = 0.0;
        for (size_t t = 0; t + 2 < indices.size(); t += 3) {
            unsigned a = indices[t], b = indices[t + 1], c = indices[t + 2];
            if (a >= uvs.size() || b >= uvs.size() || c >= uvs.size()) continue;
            float worldArea = 0.5f * cross(verts[b] - verts[a], verts[c] - verts[a]).length();
            if (worldArea < 1e-12f) continue;
            float du1 = uvs[b].u - uvs[a].u, dv1 = uvs[b].v - uvs[a].v;
            float du2 = uvs[c].u - uvs[a].u, dv2 = uvs[c].v - uvs[a].v;
            float uvArea = 0.5f * std::abs(du1 * dv2 - du2 * dv1);
            float density = std::sqrt(uvArea) * static_cast<float>(textureSize) /
                            std::sqrt(worldArea);
            weightedSum += static_cast<double>(density) * worldArea;
            totalWorld += worldArea;
        }
        return (totalWorld > 1e-12f) ? static_cast<float>(weightedSum / totalWorld) : 0.0f;
    }

private:
    static constexpr float PI = 3.14159265358979323846f;

    static float dot(const Vector3& a, const Vector3& b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }
    static Vector3 cross(const Vector3& a, const Vector3& b) {
        return {a.y * b.z - a.z * b.y,
                a.z * b.x - a.x * b.z,
                a.x * b.y - a.y * b.x};
    }
};

} // namespace nexus::utility::graphics
