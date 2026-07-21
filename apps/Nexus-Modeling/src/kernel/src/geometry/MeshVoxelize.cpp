#include <nexus/geometry/MeshVoxelize.h>
#include <nexus/geometry/MeshBVH.h>
#include <nexus/geometry/Tolerance.h>  // geometry::isFinite (non-finite rejection convention)

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

namespace {

struct Aabb {
    Vec3 mn, mx;
};

bool aabbOverlap(const Aabb& a, const Aabb& b) {
    return (a.mn.x <= b.mx.x && a.mx.x >= b.mn.x) &&
           (a.mn.y <= b.mx.y && a.mx.y >= b.mn.y) &&
           (a.mn.z <= b.mx.z && a.mx.z >= b.mn.z);
}

Aabb voxelAabb(uint32_t ix, uint32_t iy, uint32_t iz,
               const Vec3& origin, const Vec3& vs) {
    Vec3 corner = {origin.x + static_cast<float>(ix) * vs.x,
                   origin.y + static_cast<float>(iy) * vs.y,
                   origin.z + static_cast<float>(iz) * vs.z};
    return {corner, {corner.x + vs.x, corner.y + vs.y, corner.z + vs.z}};
}

bool satOverlap(const Vec3& v0, const Vec3& v1, const Vec3& v2,
                const Aabb& box) {
    Vec3 f[3] = {v0, v1, v2};
    Vec3 e[3] = {v1 - v0, v2 - v1, v0 - v2};
    Vec3 boxExtent = (box.mx - box.mn) * 0.5f;
    Vec3 boxCenter = (box.mn + box.mx) * 0.5f;

    Vec3 fb[3];
    for (int i = 0; i < 3; ++i)
        fb[i] = f[i] - boxCenter;

    for (int i = 0; i < 3; ++i) {
        float r = boxExtent.x * std::fabs(e[i].x)
                + boxExtent.y * std::fabs(e[i].y)
                + boxExtent.z * std::fabs(e[i].z);
        float minProj = fb[0].dot(e[i]);
        float maxProj = minProj;
        for (int j = 1; j < 3; ++j) {
            float d = fb[j].dot(e[i]);
            if (d < minProj) minProj = d;
            if (d > maxProj) maxProj = d;
        }
        if (std::min(maxProj, r) + std::max(minProj, -r) < -1e-8f) return false;
    }

    for (int i = 0; i < 3; ++i) {
        float r = boxExtent.x * std::fabs(e[i].x)
                + boxExtent.y * std::fabs(e[i].y)
                + boxExtent.z * std::fabs(e[i].z);
        float minProj = fb[0].dot(f[i]);
        float maxProj = minProj;
        for (int j = 1; j < 3; ++j) {
            float d = fb[j].dot(f[i]);
            if (d < minProj) minProj = d;
            if (d > maxProj) maxProj = d;
        }
        if (std::min(maxProj, r) + std::max(minProj, -r) < -1e-8f) return false;
    }

    Vec3 boxAxes[3] = {{1,0,0}, {0,1,0}, {0,0,1}};
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            Vec3 axis = e[i].cross(boxAxes[j]);
            float lenSq = axis.lengthSq();
            if (lenSq < 1e-12f) continue;
            axis = axis * (1.f / std::sqrt(lenSq));

            float r = boxExtent.x * std::fabs(axis.x)
                      + boxExtent.y * std::fabs(axis.y)
                      + boxExtent.z * std::fabs(axis.z);

            float minProj = fb[0].dot(axis);
            float maxProj = minProj;
            for (int k = 1; k < 3; ++k) {
                float d = fb[k].dot(axis);
                if (d < minProj) minProj = d;
                if (d > maxProj) maxProj = d;
            }
            if (std::min(maxProj, r) + std::max(minProj, -r) < -1e-8f) return false;
        }
    }

    {
        Vec3 n = (v1 - v0).cross(v2 - v0);
        float minDist = fb[0].dot(n);
        float maxDist = minDist;
        for (int i = 1; i < 3; ++i) {
            float dist = fb[i].dot(n);
            if (dist < minDist) minDist = dist;
            if (dist > maxDist) maxDist = dist;
        }
        float r = boxExtent.x * std::fabs(n.x)
                + boxExtent.y * std::fabs(n.y)
                + boxExtent.z * std::fabs(n.z);
        if (std::min(maxDist, r) + std::max(minDist, -r) < -1e-8f) return false;
    }

    return true;
}

} // namespace

VoxelGrid MeshVoxelize::voxelize(const Mesh& mesh, const VoxelizeOptions& opts) {
    // Reject non-finite input rather than rasterize NaN/±Inf geometry into a
    // finite-but-garbage grid (the non-finite-rejection convention: empty on bad input).
    for (const auto& p : mesh.attributes().positions())
        if (!isFinite(p)) return VoxelGrid{};

    VoxelGrid grid;
    grid.resolution = opts.resolution;
    grid.origin = opts.origin;
    grid.voxelSize = opts.voxelSize;

    size_t total = static_cast<size_t>(opts.resolution[0])
                 * static_cast<size_t>(opts.resolution[1])
                 * static_cast<size_t>(opts.resolution[2]);
    grid.occupancy.assign(total, false);

    if (!mesh.isValid()) return grid;

    MeshBVH bvh;
    bvh.build(mesh);
    if (!bvh.isValid()) return grid;

    const auto& nodes = bvh.nodes();
    const auto& tris = bvh.tris();

    std::vector<int32_t> stack;
    stack.reserve(128);

    for (uint32_t iz = 0; iz < opts.resolution[2]; ++iz) {
        for (uint32_t iy = 0; iy < opts.resolution[1]; ++iy) {
            for (uint32_t ix = 0; ix < opts.resolution[0]; ++ix) {
                Aabb vBox = voxelAabb(ix, iy, iz, opts.origin, opts.voxelSize);

                stack.clear();
                stack.push_back(0);

                bool occupied = false;

                while (!stack.empty() && !occupied) {
                    int32_t nodeIdx = stack.back();
                    stack.pop_back();

                    if (nodeIdx < 0 || static_cast<size_t>(nodeIdx) >= nodes.size())
                        continue;

                    const auto& node = nodes[static_cast<size_t>(nodeIdx)];
                    Aabb nodeBox{node.min, node.max};

                    if (!aabbOverlap(vBox, nodeBox)) continue;

                    if (node.isLeaf) {
                        for (int32_t ti = 0; ti < node.triCount; ++ti) {
                            const auto& tri = tris[static_cast<size_t>(node.firstTri + ti)];
                            Vec3 v1 = {tri.v0.x + tri.e1.x,
                                       tri.v0.y + tri.e1.y,
                                       tri.v0.z + tri.e1.z};
                            Vec3 v2 = {tri.v0.x + tri.e2.x,
                                       tri.v0.y + tri.e2.y,
                                       tri.v0.z + tri.e2.z};
                            if (satOverlap(tri.v0, v1, v2, vBox)) {
                                occupied = true;
                                break;
                            }
                        }
                    } else {
                        if (node.leftChild >= 0)
                            stack.push_back(node.leftChild);
                        if (node.firstTri >= 0)
                            stack.push_back(node.firstTri);
                    }
                }

                if (occupied) {
                    grid.occupancy[grid.index(ix, iy, iz)] = true;
                }
            }
        }
    }

    return grid;
}

} // namespace nexus::geometry
