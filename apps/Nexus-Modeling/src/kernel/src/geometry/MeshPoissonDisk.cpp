#include <nexus/geometry/MeshPoissonDisk.h>
#include <nexus/geometry/MeshPointSample.h>

#include <limits>
#include <nexus/geometry/MeshBVH.h>
#include "SplitMix64.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

PoissonDiskResult MeshPoissonDisk::sample(const Mesh& mesh, const PoissonDiskOptions& opts) {
    PoissonDiskResult result;
    if (!mesh.isValid() || opts.minDist <= 0.f) return result;

    MeshBVH bvh;
    bvh.build(mesh);

    const uint32_t candidatePoolSize = opts.maxAttempts * 8;
    // maxPoints == 0 means "as many as the spacing allows". It used to fall back to the
    // candidate POOL size — maxAttempts * 8, or 240 by default — which is a property of the
    // seeding, not of the surface, so any request fine enough to need more than 240 points
    // silently returned exactly 240. Dart-throwing terminates on its own when no active
    // point can accept another neighbour, so no artificial ceiling is needed.
    const uint32_t maxOut =
        (opts.maxPoints > 0) ? opts.maxPoints : std::numeric_limits<uint32_t>::max();
    PointSampleOptions sampOpts;
    sampOpts.count = candidatePoolSize;
    sampOpts.seed = opts.seed;
    auto candResult = MeshPointSample::sample(mesh, sampOpts);

    if (candResult.points.empty()) return result;

    internal::SplitMix64 rng(opts.seed + 1);

    struct Candidate {
        Vec3 point;
        Vec3 normal;
        uint32_t triangle;
    };
    std::vector<Candidate> candidates;
    candidates.reserve(candResult.points.size());
    for (size_t i = 0; i < candResult.points.size(); ++i) {
        candidates.push_back({candResult.points[i], candResult.normals[i], candResult.triangles[i]});
    }

    for (size_t i = candidates.size() - 1; i > 0; --i) {
        size_t j = static_cast<size_t>(rng.next() % (i + 1));
        std::swap(candidates[i], candidates[j]);
    }

    const float cellSize = opts.minDist / std::sqrt(3.f);
    const float cellSizeInv = 1.f / cellSize;
    const float minDistSq = opts.minDist * opts.minDist;

    const Vec3& firstPt = candidates[0].point;
    Vec3 gridMin = firstPt;
    Vec3 gridMax = firstPt;

    for (const auto& c : candidates) {
        gridMin.x = std::min(gridMin.x, c.point.x);
        gridMin.y = std::min(gridMin.y, c.point.y);
        gridMin.z = std::min(gridMin.z, c.point.z);
        gridMax.x = std::max(gridMax.x, c.point.x);
        gridMax.y = std::max(gridMax.y, c.point.y);
        gridMax.z = std::max(gridMax.z, c.point.z);
    }

    gridMin.x -= opts.minDist * 2.f;
    gridMin.y -= opts.minDist * 2.f;
    gridMin.z -= opts.minDist * 2.f;
    gridMax.x += opts.minDist * 2.f;
    gridMax.y += opts.minDist * 2.f;
    gridMax.z += opts.minDist * 2.f;

    int gx = std::max(1, static_cast<int>((gridMax.x - gridMin.x) * cellSizeInv) + 1);
    int gy = std::max(1, static_cast<int>((gridMax.y - gridMin.y) * cellSizeInv) + 1);
    int gz = std::max(1, static_cast<int>((gridMax.z - gridMin.z) * cellSizeInv) + 1);

    std::vector<int32_t> grid(static_cast<size_t>(gx) * static_cast<size_t>(gy) * static_cast<size_t>(gz), -1);

    auto cellIdx = [&](const Vec3& pt) -> int {
        int ix = static_cast<int>((pt.x - gridMin.x) * cellSizeInv);
        int iy = static_cast<int>((pt.y - gridMin.y) * cellSizeInv);
        int iz = static_cast<int>((pt.z - gridMin.z) * cellSizeInv);
        ix = std::clamp(ix, 0, gx - 1);
        iy = std::clamp(iy, 0, gy - 1);
        iz = std::clamp(iz, 0, gz - 1);
        return ix + iy * gx + iz * gx * gy;
    };

    auto projectToSurface = [&](const Vec3& pt) -> Candidate {
        if (bvh.isValid()) {
            auto hit = bvh.closestPoint(pt);
            if (hit.valid) {
                const auto& pos = mesh.attributes().positions();
                const auto& topo = mesh.topology();
                Vec3 n = {0.f, 0.f, 1.f};
                if (hit.triangleIndex < static_cast<uint32_t>(topo.faceCount())) {
                    const auto& f = topo.face(hit.triangleIndex);
                    if (f.vertexCount() >= 3 && f.indicesInBounds(pos.size())) {
                        const Vec3& a = pos[f.indices[0]];
                        const Vec3& b = pos[f.indices[1]];
                        const Vec3& c = pos[f.indices[2]];
                        Vec3 e1 = {b.x - a.x, b.y - a.y, b.z - a.z};
                        Vec3 e2 = {c.x - a.x, c.y - a.y, c.z - a.z};
                        float nx = e1.y * e2.z - e1.z * e2.y;
                        float ny = e1.z * e2.x - e1.x * e2.z;
                        float nz = e1.x * e2.y - e1.y * e2.x;
                        float len = std::sqrt(nx*nx + ny*ny + nz*nz);
                        if (len > 1e-10f) { n = {nx/len, ny/len, nz/len}; }
                    }
                }
                Candidate c;
                c.point = hit.point;
                c.normal = n;
                c.triangle = hit.triangleIndex;
                return c;
            }
        }
        Candidate c;
        c.point = pt;
        c.normal = {0.f, 0.f, 1.f};
        c.triangle = 0;
        return c;
    };

    std::vector<int32_t> activeList;
    std::vector<Vec3> placed;
    std::vector<Vec3> placedNormals;

    Candidate seed = candidates[0];
    int32_t firstIdx = static_cast<int32_t>(placed.size());
    placed.push_back(seed.point);
    placedNormals.push_back(seed.normal);
    result.points.push_back(seed.point);
    result.normals.push_back(seed.normal);
    result.triangles.push_back(seed.triangle);
    grid[static_cast<size_t>(cellIdx(seed.point))] = firstIdx;
    activeList.push_back(firstIdx);

    while (!activeList.empty() && result.points.size() < maxOut) {
        size_t ai = static_cast<size_t>(rng.next() % activeList.size());
        int32_t baseIdx = activeList[ai];
        const Vec3& base = placed[static_cast<size_t>(baseIdx)];
        const Vec3& baseNormal = placedNormals[static_cast<size_t>(baseIdx)];

        Vec3 u, v;
        if (std::abs(baseNormal.x) < std::abs(baseNormal.y) && std::abs(baseNormal.x) < std::abs(baseNormal.z)) {
            u = {0.f, -baseNormal.z, baseNormal.y};
        } else if (std::abs(baseNormal.y) < std::abs(baseNormal.z)) {
            u = {-baseNormal.z, 0.f, baseNormal.x};
        } else {
            u = {-baseNormal.y, baseNormal.x, 0.f};
        }
        float ulen = std::sqrt(u.x*u.x + u.y*u.y + u.z*u.z);
        if (ulen > 0.f) { u = {u.x/ulen, u.y/ulen, u.z/ulen}; }
        else { u = {1.f, 0.f, 0.f}; }
        v = {baseNormal.y * u.z - baseNormal.z * u.y,
             baseNormal.z * u.x - baseNormal.x * u.z,
             baseNormal.x * u.y - baseNormal.y * u.x};

        bool accepted = false;
        for (uint32_t attempt = 0; attempt < opts.maxAttempts; ++attempt) {
            float angle = static_cast<float>(rng.uniform01() * 2.0 * 3.14159265358979323846);
            float dist = opts.minDist * (1.f + static_cast<float>(rng.uniform01()));

            float cosA = std::cos(angle);
            float sinA = std::sin(angle);

            Vec3 volPt = {
                base.x + dist * (cosA * u.x + sinA * v.x),
                base.y + dist * (cosA * u.y + sinA * v.y),
                base.z + dist * (cosA * u.z + sinA * v.z),
            };

            Candidate newCand = projectToSurface(volPt);
            Vec3 newPt = newCand.point;

            int cx = std::clamp(static_cast<int>((newPt.x - gridMin.x) * cellSizeInv), 0, gx - 1);
            int cy = std::clamp(static_cast<int>((newPt.y - gridMin.y) * cellSizeInv), 0, gy - 1);
            int cz = std::clamp(static_cast<int>((newPt.z - gridMin.z) * cellSizeInv), 0, gz - 1);

            bool conflict = false;
            for (int dz = -2; dz <= 2 && !conflict; ++dz) {
                for (int dy = -2; dy <= 2 && !conflict; ++dy) {
                    for (int dx = -2; dx <= 2 && !conflict; ++dx) {
                        int nx = cx + dx;
                        int ny = cy + dy;
                        int nz = cz + dz;
                        if (nx < 0 || nx >= gx || ny < 0 || ny >= gy || nz < 0 || nz >= gz) continue;
                        int ni = nx + ny * gx + nz * gx * gy;
                        int32_t pidx = grid[static_cast<size_t>(ni)];
                        if (pidx < 0) continue;

                        const Vec3& op = placed[static_cast<size_t>(pidx)];
                        float dxv = newPt.x - op.x;
                        float dyv = newPt.y - op.y;
                        float dzv = newPt.z - op.z;
                        if (dxv * dxv + dyv * dyv + dzv * dzv < minDistSq) {
                            conflict = true;
                            break;
                        }
                    }
                }
            }

            if (!conflict) {
                int32_t newIdx = static_cast<int32_t>(placed.size());
                placed.push_back(newPt);
                placedNormals.push_back(newCand.normal);
                result.points.push_back(newPt);
                result.normals.push_back(newCand.normal);
                result.triangles.push_back(newCand.triangle);
                grid[static_cast<size_t>(cellIdx(newPt))] = newIdx;
                activeList.push_back(newIdx);
                accepted = true;
                break;
            }
        }

        if (!accepted) {
            activeList.erase(activeList.begin() + static_cast<std::ptrdiff_t>(ai));
        }
    }

    return result;
}

} // namespace nexus::geometry
