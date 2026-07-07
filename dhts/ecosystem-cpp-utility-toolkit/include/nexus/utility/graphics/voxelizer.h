#pragma once

#include <vector>
#include <array>
#include <cstdint>
#include <cmath>
#include <limits>
#include <algorithm>
#include <unordered_map>
#include <nexus/utility/math/vector3_utils.h>

namespace nexus::utility::graphics {
using Vector3 = nexus::utility::math::Vector3;

/// @brief Conversion between triangle meshes and voxel grids.
///
/// Voxelization rasterizes each triangle against the cells inside its bounding
/// box using the Akenine-Möller triangle/box overlap test (separating-axis
/// theorem). Surface extraction emits the exposed faces of solid voxels as
/// welded quads split into triangles. Additional utilities flood-fill enclosed
/// cavities, apply spherical erosion/dilation, and build a sparse voxel octree
/// with a child-mask/child-pointer node layout.
class Voxelizer {
public:
    using Vector3 = nexus::utility::math::Vector3;

    struct VoxelGrid {
        Vector3 origin{0.0f, 0.0f, 0.0f};
        float cellSize = 1.0f;
        int nx = 0, ny = 0, nz = 0;
        std::vector<uint8_t> data;

        size_t index(int i, int j, int k) const {
            return static_cast<size_t>(i) +
                   static_cast<size_t>(nx) * (static_cast<size_t>(j) +
                   static_cast<size_t>(ny) * static_cast<size_t>(k));
        }
        bool inBounds(int i, int j, int k) const {
            return i >= 0 && j >= 0 && k >= 0 && i < nx && j < ny && k < nz;
        }
        uint8_t at(int i, int j, int k) const {
            return inBounds(i, j, k) ? data[index(i, j, k)] : uint8_t(0);
        }
        void set(int i, int j, int k, uint8_t v) {
            if (inBounds(i, j, k)) data[index(i, j, k)] = v;
        }
        Vector3 cellCenter(int i, int j, int k) const {
            return Vector3(origin.x + (static_cast<float>(i) + 0.5f) * cellSize,
                           origin.y + (static_cast<float>(j) + 0.5f) * cellSize,
                           origin.z + (static_cast<float>(k) + 0.5f) * cellSize);
        }
        Vector3 corner(int i, int j, int k) const {
            return Vector3(origin.x + static_cast<float>(i) * cellSize,
                           origin.y + static_cast<float>(j) * cellSize,
                           origin.z + static_cast<float>(k) * cellSize);
        }
    };

    struct Mesh {
        std::vector<Vector3> verts;
        std::vector<unsigned> indices;
    };

    struct SparseVoxelOctree {
        struct Node {
            uint32_t childPtr = 0;
            uint8_t childMask = 0;
        };
        std::vector<Node> nodes;
        int side = 0;
        Vector3 origin{0.0f, 0.0f, 0.0f};
        float cellSize = 1.0f;
    };

    // ── Voxelization ────────────────────────────────────────────────────

    /// @brief Voxelize a triangle mesh; `resolution` voxels span the longest axis.
    static VoxelGrid voxelize(const std::vector<Vector3>& verts,
                              const std::vector<unsigned>& indices,
                              int resolution) {
        VoxelGrid grid;
        if (verts.empty() || resolution < 1) return grid;

        Vector3 lo = verts[0], hi = verts[0];
        for (const auto& v : verts) {
            lo.x = std::min(lo.x, v.x); lo.y = std::min(lo.y, v.y); lo.z = std::min(lo.z, v.z);
            hi.x = std::max(hi.x, v.x); hi.y = std::max(hi.y, v.y); hi.z = std::max(hi.z, v.z);
        }
        Vector3 ext = hi - lo;
        float maxExt = std::max({ext.x, ext.y, ext.z, 1e-6f});
        float cell = maxExt / static_cast<float>(resolution);

        grid.origin = lo;
        grid.cellSize = cell;
        grid.nx = std::max(1, static_cast<int>(std::ceil(ext.x / cell)) + 1);
        grid.ny = std::max(1, static_cast<int>(std::ceil(ext.y / cell)) + 1);
        grid.nz = std::max(1, static_cast<int>(std::ceil(ext.z / cell)) + 1);
        grid.data.assign(static_cast<size_t>(grid.nx) * grid.ny * grid.nz, 0);

        rasterize(grid, verts, indices);
        return grid;
    }

    // ── Surface extraction ──────────────────────────────────────────────

    /// @brief Extract the exposed surface of solid voxels as a triangle mesh.
    static Mesh toMesh(const VoxelGrid& grid) {
        Mesh mesh;
        struct CKey {
            int x, y, z;
            bool operator==(const CKey& o) const { return x == o.x && y == o.y && z == o.z; }
        };
        struct CHash {
            size_t operator()(const CKey& k) const {
                size_t h = 1469598103934665603ULL;
                auto mix = [&](int v) {
                    h ^= static_cast<size_t>(static_cast<uint32_t>(v));
                    h *= 1099511628211ULL;
                };
                mix(k.x); mix(k.y); mix(k.z);
                return h;
            }
        };
        std::unordered_map<CKey, unsigned, CHash> weld;
        auto vertexAt = [&](int cx, int cy, int cz) -> unsigned {
            CKey k{cx, cy, cz};
            auto it = weld.find(k);
            if (it != weld.end()) return it->second;
            unsigned id = static_cast<unsigned>(mesh.verts.size());
            mesh.verts.push_back(grid.corner(cx, cy, cz));
            weld.emplace(k, id);
            return id;
        };

        // 6 face directions with neighbor offset and 4 CCW (outward) corner offsets.
        static const int normalOff[6][3] = {
            {-1, 0, 0}, {1, 0, 0}, {0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}};
        static const int faceCorners[6][4][3] = {
            {{0, 0, 0}, {0, 0, 1}, {0, 1, 1}, {0, 1, 0}},  // -X
            {{1, 0, 0}, {1, 1, 0}, {1, 1, 1}, {1, 0, 1}},  // +X
            {{0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {0, 0, 1}},  // -Y
            {{0, 1, 0}, {0, 1, 1}, {1, 1, 1}, {1, 1, 0}},  // +Y
            {{0, 0, 0}, {0, 1, 0}, {1, 1, 0}, {1, 0, 0}},  // -Z
            {{0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}}}; // +Z

        for (int k = 0; k < grid.nz; ++k)
            for (int j = 0; j < grid.ny; ++j)
                for (int i = 0; i < grid.nx; ++i) {
                    if (!grid.at(i, j, k)) continue;
                    for (int d = 0; d < 6; ++d) {
                        int ni = i + normalOff[d][0];
                        int nj = j + normalOff[d][1];
                        int nk = k + normalOff[d][2];
                        if (grid.at(ni, nj, nk)) continue;  // face not exposed
                        unsigned q[4];
                        for (int c = 0; c < 4; ++c)
                            q[c] = vertexAt(i + faceCorners[d][c][0],
                                            j + faceCorners[d][c][1],
                                            k + faceCorners[d][c][2]);
                        mesh.indices.push_back(q[0]);
                        mesh.indices.push_back(q[1]);
                        mesh.indices.push_back(q[2]);
                        mesh.indices.push_back(q[0]);
                        mesh.indices.push_back(q[2]);
                        mesh.indices.push_back(q[3]);
                    }
                }
        return mesh;
    }

    // ── Cavity filling ──────────────────────────────────────────────────

    /// @brief Fill enclosed empty regions by flood-filling exterior air inward.
    static void fillInterior(VoxelGrid& grid) {
        const size_t N = grid.data.size();
        if (N == 0) return;
        std::vector<uint8_t> outside(N, 0);
        std::vector<int> stack;

        auto push = [&](int i, int j, int k) {
            if (!grid.inBounds(i, j, k)) return;
            size_t idx = grid.index(i, j, k);
            if (grid.data[idx] || outside[idx]) return;
            outside[idx] = 1;
            stack.push_back(static_cast<int>(idx));
        };

        for (int j = 0; j < grid.ny; ++j)
            for (int i = 0; i < grid.nx; ++i) { push(i, j, 0); push(i, j, grid.nz - 1); }
        for (int k = 0; k < grid.nz; ++k)
            for (int i = 0; i < grid.nx; ++i) { push(i, 0, k); push(i, grid.ny - 1, k); }
        for (int k = 0; k < grid.nz; ++k)
            for (int j = 0; j < grid.ny; ++j) { push(0, j, k); push(grid.nx - 1, j, k); }

        while (!stack.empty()) {
            int idx = stack.back();
            stack.pop_back();
            int i = idx % grid.nx;
            int j = (idx / grid.nx) % grid.ny;
            int k = idx / (grid.nx * grid.ny);
            push(i - 1, j, k); push(i + 1, j, k);
            push(i, j - 1, k); push(i, j + 1, k);
            push(i, j, k - 1); push(i, j, k + 1);
        }

        for (size_t idx = 0; idx < N; ++idx)
            if (!grid.data[idx] && !outside[idx]) grid.data[idx] = 1;
    }

    // ── Morphology ──────────────────────────────────────────────────────

    /// @brief Spherical dilation by `radius` voxels.
    static VoxelGrid dilate(const VoxelGrid& grid, int radius) {
        return morph(grid, radius, true);
    }
    /// @brief Spherical erosion by `radius` voxels.
    static VoxelGrid erode(const VoxelGrid& grid, int radius) {
        return morph(grid, radius, false);
    }

    // ── Sparse voxel octree ─────────────────────────────────────────────

    /// @brief Build a sparse voxel octree from a mesh voxelized at 2^maxDepth.
    static SparseVoxelOctree sparseOctree(const std::vector<Vector3>& verts,
                                          const std::vector<unsigned>& indices,
                                          int maxDepth) {
        SparseVoxelOctree svo;
        if (maxDepth < 1) maxDepth = 1;
        int side = 1 << maxDepth;

        VoxelGrid grid = voxelizeCubic(verts, indices, side);
        svo.side = side;
        svo.origin = grid.origin;
        svo.cellSize = grid.cellSize;

        svo.nodes.push_back({0, 0});  // root placeholder
        buildOctreeNode(grid, svo, 0, 0, 0, 0, side);
        return svo;
    }

private:
    static float dot(const Vector3& a, const Vector3& b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }
    static Vector3 cross(const Vector3& a, const Vector3& b) {
        return {a.y * b.z - a.z * b.y,
                a.z * b.x - a.x * b.z,
                a.x * b.y - a.y * b.x};
    }

    static VoxelGrid voxelizeCubic(const std::vector<Vector3>& verts,
                                   const std::vector<unsigned>& indices,
                                   int side) {
        VoxelGrid grid;
        if (verts.empty() || side < 1) return grid;
        Vector3 lo = verts[0], hi = verts[0];
        for (const auto& v : verts) {
            lo.x = std::min(lo.x, v.x); lo.y = std::min(lo.y, v.y); lo.z = std::min(lo.z, v.z);
            hi.x = std::max(hi.x, v.x); hi.y = std::max(hi.y, v.y); hi.z = std::max(hi.z, v.z);
        }
        Vector3 ext = hi - lo;
        float maxExt = std::max({ext.x, ext.y, ext.z, 1e-6f});
        float cell = maxExt / static_cast<float>(side);
        // Center the mesh in a cubic grid so all axes share `side` voxels.
        Vector3 center = (lo + hi) * 0.5f;
        float halfSpan = 0.5f * static_cast<float>(side) * cell;
        grid.origin = Vector3(center.x - halfSpan, center.y - halfSpan, center.z - halfSpan);
        grid.cellSize = cell;
        grid.nx = grid.ny = grid.nz = side;
        grid.data.assign(static_cast<size_t>(side) * side * side, 0);
        rasterize(grid, verts, indices);
        return grid;
    }

    static void rasterize(VoxelGrid& grid, const std::vector<Vector3>& verts,
                          const std::vector<unsigned>& indices) {
        Vector3 half(grid.cellSize * 0.5f, grid.cellSize * 0.5f, grid.cellSize * 0.5f);
        for (size_t t = 0; t + 2 < indices.size(); t += 3) {
            const Vector3& a = verts[indices[t]];
            const Vector3& b = verts[indices[t + 1]];
            const Vector3& c = verts[indices[t + 2]];

            Vector3 tlo(std::min({a.x, b.x, c.x}), std::min({a.y, b.y, c.y}),
                        std::min({a.z, b.z, c.z}));
            Vector3 thi(std::max({a.x, b.x, c.x}), std::max({a.y, b.y, c.y}),
                        std::max({a.z, b.z, c.z}));

            int i0 = std::max(0, static_cast<int>(std::floor((tlo.x - grid.origin.x) / grid.cellSize)));
            int j0 = std::max(0, static_cast<int>(std::floor((tlo.y - grid.origin.y) / grid.cellSize)));
            int k0 = std::max(0, static_cast<int>(std::floor((tlo.z - grid.origin.z) / grid.cellSize)));
            int i1 = std::min(grid.nx - 1, static_cast<int>(std::ceil((thi.x - grid.origin.x) / grid.cellSize)));
            int j1 = std::min(grid.ny - 1, static_cast<int>(std::ceil((thi.y - grid.origin.y) / grid.cellSize)));
            int k1 = std::min(grid.nz - 1, static_cast<int>(std::ceil((thi.z - grid.origin.z) / grid.cellSize)));

            for (int k = k0; k <= k1; ++k)
                for (int j = j0; j <= j1; ++j)
                    for (int i = i0; i <= i1; ++i) {
                        Vector3 center = grid.cellCenter(i, j, k);
                        if (triBoxOverlap(center, half, a, b, c))
                            grid.data[grid.index(i, j, k)] = 1;
                    }
        }
    }

    static VoxelGrid morph(const VoxelGrid& grid, int radius, bool dilateOp) {
        VoxelGrid out = grid;
        if (radius < 1) return out;
        std::fill(out.data.begin(), out.data.end(), uint8_t(0));

        std::vector<std::array<int, 3>> offsets;
        for (int dz = -radius; dz <= radius; ++dz)
            for (int dy = -radius; dy <= radius; ++dy)
                for (int dx = -radius; dx <= radius; ++dx)
                    if (dx * dx + dy * dy + dz * dz <= radius * radius)
                        offsets.push_back({dx, dy, dz});

        for (int k = 0; k < grid.nz; ++k)
            for (int j = 0; j < grid.ny; ++j)
                for (int i = 0; i < grid.nx; ++i) {
                    if (dilateOp) {
                        uint8_t v = 0;
                        for (const auto& o : offsets)
                            if (grid.at(i + o[0], j + o[1], k + o[2])) { v = 1; break; }
                        out.data[out.index(i, j, k)] = v;
                    } else {
                        uint8_t v = 1;
                        for (const auto& o : offsets) {
                            int ni = i + o[0], nj = j + o[1], nk = k + o[2];
                            if (!grid.inBounds(ni, nj, nk) || !grid.at(ni, nj, nk)) {
                                v = 0;
                                break;
                            }
                        }
                        out.data[out.index(i, j, k)] = v;
                    }
                }
        return out;
    }

    // Any solid voxel inside the cube [x0,x0+side) x [y0,..) x [z0,..)?
    static bool anySolid(const VoxelGrid& grid, int x0, int y0, int z0, int side) {
        int x1 = std::min(x0 + side, grid.nx);
        int y1 = std::min(y0 + side, grid.ny);
        int z1 = std::min(z0 + side, grid.nz);
        for (int k = z0; k < z1; ++k)
            for (int j = y0; j < y1; ++j)
                for (int i = x0; i < x1; ++i)
                    if (grid.data[grid.index(i, j, k)]) return true;
        return false;
    }

    static int popcount8(uint8_t m) {
        int c = 0;
        while (m) { c += (m & 1); m >>= 1; }
        return c;
    }

    static void buildOctreeNode(const VoxelGrid& grid, SparseVoxelOctree& svo,
                                uint32_t self, int x0, int y0, int z0, int side) {
        if (side == 2) {
            uint8_t mask = 0;
            for (int o = 0; o < 8; ++o) {
                int dx = o & 1, dy = (o >> 1) & 1, dz = (o >> 2) & 1;
                if (grid.at(x0 + dx, y0 + dy, z0 + dz)) mask |= static_cast<uint8_t>(1 << o);
            }
            svo.nodes[self].childMask = mask;
            svo.nodes[self].childPtr = 0;
            return;
        }

        int half = side / 2;
        uint8_t mask = 0;
        bool occ[8];
        for (int o = 0; o < 8; ++o) {
            int dx = (o & 1) * half, dy = ((o >> 1) & 1) * half, dz = ((o >> 2) & 1) * half;
            occ[o] = anySolid(grid, x0 + dx, y0 + dy, z0 + dz, half);
            if (occ[o]) mask |= static_cast<uint8_t>(1 << o);
        }

        uint32_t base = static_cast<uint32_t>(svo.nodes.size());
        svo.nodes[self].childMask = mask;
        svo.nodes[self].childPtr = base;

        int count = popcount8(mask);
        for (int c = 0; c < count; ++c) svo.nodes.push_back({0, 0});

        int ci = 0;
        for (int o = 0; o < 8; ++o) {
            if (!occ[o]) continue;
            int dx = (o & 1) * half, dy = ((o >> 1) & 1) * half, dz = ((o >> 2) & 1) * half;
            buildOctreeNode(grid, svo, base + ci, x0 + dx, y0 + dy, z0 + dz, half);
            ++ci;
        }
    }

    // ── Triangle/box overlap (Akenine-Möller SAT) ───────────────────────

    static bool axisTest(float a, float b, float fa, float fb,
                         float v0a, float v0b, float v1a, float v1b,
                         float halfA, float halfB) {
        float p0 = a * v0a + b * v0b;
        float p1 = a * v1a + b * v1b;
        float mn = std::min(p0, p1);
        float mx = std::max(p0, p1);
        float rad = fa * halfA + fb * halfB;
        return !(mn > rad || mx < -rad);
    }

    static bool triBoxOverlap(const Vector3& boxCenter, const Vector3& boxHalf,
                              const Vector3& t0, const Vector3& t1, const Vector3& t2) {
        Vector3 v0 = t0 - boxCenter;
        Vector3 v1 = t1 - boxCenter;
        Vector3 v2 = t2 - boxCenter;

        Vector3 e0 = v1 - v0;
        Vector3 e1 = v2 - v1;
        Vector3 e2 = v0 - v2;

        // 9 edge-normal axis tests.
        {
            float fex = std::abs(e0.x), fey = std::abs(e0.y), fez = std::abs(e0.z);
            if (!axisTest(e0.z, -e0.y, fez, fey, v0.y, v0.z, v2.y, v2.z, boxHalf.y, boxHalf.z)) return false;
            if (!axisTest(-e0.z, e0.x, fez, fex, v0.x, v0.z, v2.x, v2.z, boxHalf.x, boxHalf.z)) return false;
            if (!axisTest(e0.y, -e0.x, fey, fex, v1.x, v1.y, v2.x, v2.y, boxHalf.x, boxHalf.y)) return false;
        }
        {
            float fex = std::abs(e1.x), fey = std::abs(e1.y), fez = std::abs(e1.z);
            if (!axisTest(e1.z, -e1.y, fez, fey, v0.y, v0.z, v2.y, v2.z, boxHalf.y, boxHalf.z)) return false;
            if (!axisTest(-e1.z, e1.x, fez, fex, v0.x, v0.z, v2.x, v2.z, boxHalf.x, boxHalf.z)) return false;
            if (!axisTest(e1.y, -e1.x, fey, fex, v0.x, v0.y, v1.x, v1.y, boxHalf.x, boxHalf.y)) return false;
        }
        {
            float fex = std::abs(e2.x), fey = std::abs(e2.y), fez = std::abs(e2.z);
            if (!axisTest(e2.z, -e2.y, fez, fey, v0.y, v0.z, v1.y, v1.z, boxHalf.y, boxHalf.z)) return false;
            if (!axisTest(-e2.z, e2.x, fez, fex, v0.x, v0.z, v1.x, v1.z, boxHalf.x, boxHalf.z)) return false;
            if (!axisTest(e2.y, -e2.x, fey, fex, v1.x, v1.y, v2.x, v2.y, boxHalf.x, boxHalf.y)) return false;
        }

        // 3 AABB overlap tests.
        auto rangeTest = [](float a, float b, float c, float half) {
            float mn = std::min({a, b, c});
            float mx = std::max({a, b, c});
            return !(mn > half || mx < -half);
        };
        if (!rangeTest(v0.x, v1.x, v2.x, boxHalf.x)) return false;
        if (!rangeTest(v0.y, v1.y, v2.y, boxHalf.y)) return false;
        if (!rangeTest(v0.z, v1.z, v2.z, boxHalf.z)) return false;

        // Triangle plane / box overlap.
        Vector3 normal = cross(e0, e1);
        float d = -dot(normal, v0);
        Vector3 vmin, vmax;
        vmin.x = (normal.x > 0.0f) ? -boxHalf.x : boxHalf.x;
        vmax.x = (normal.x > 0.0f) ? boxHalf.x : -boxHalf.x;
        vmin.y = (normal.y > 0.0f) ? -boxHalf.y : boxHalf.y;
        vmax.y = (normal.y > 0.0f) ? boxHalf.y : -boxHalf.y;
        vmin.z = (normal.z > 0.0f) ? -boxHalf.z : boxHalf.z;
        vmax.z = (normal.z > 0.0f) ? boxHalf.z : -boxHalf.z;
        if (dot(normal, vmin) + d > 0.0f) return false;
        if (dot(normal, vmax) + d < 0.0f) return false;
        return true;
    }
};

} // namespace nexus::utility::graphics
