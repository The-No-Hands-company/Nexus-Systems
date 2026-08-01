#include <nexus/geometry/TetDelaunay3D.h>
#include <nexus/geometry/MeshConvexHull.h>
#include <nexus/geometry/RobustPredicates.h>

#include "DelaunaySuperTriangle.h"

#include <algorithm>
#include <array>
#include <limits>
#include <unordered_map>
#include <utility>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

namespace {

struct Tet {
    std::array<uint32_t, 4> v;

    bool isValid() const noexcept {
        return v[0] != v[1] && v[0] != v[2] && v[0] != v[3]
            && v[1] != v[2] && v[1] != v[3]
            && v[2] != v[3];
    }
};

// A tetrahedron is non-degenerate iff its four vertices are not coplanar.
bool nonDegenerate(const std::array<uint32_t, 4>& v, const std::vector<Vec3>& pts) noexcept {
    return RobustPredicates::orient3D(pts[v[0]], pts[v[1]], pts[v[2]], pts[v[3]]) != 0.0;
}

// Exact circumsphere-containment: p is strictly inside the circumsphere of tet (a,b,c,d) iff
// the exact in-sphere determinant and the tet's orientation share a sign. The old test built
// the circumcenter in float and compared squared distances with an epsilon slop, which is
// exactly where cospherical / near-cospherical configurations (a cube's corners, dense
// tessellations) misclassify and leave holes. This routes through the exact predicate.
bool pointInCircumsphere(const Vec3& p, const std::array<uint32_t, 4>& v,
                         const std::vector<Vec3>& pts) noexcept {
    const Vec3& a = pts[v[0]];
    const Vec3& b = pts[v[1]];
    const Vec3& c = pts[v[2]];
    const Vec3& d = pts[v[3]];
    const double o = RobustPredicates::orient3D(a, b, c, d);
    if (o == 0.0) return false;  // flat tet has no circumsphere
    return RobustPredicates::inSphere(a, b, c, d, p) * o > 0.0;
}

// Hash for sorted triangle (3 vertex indices).
uint64_t hashTriangle(uint32_t a, uint32_t b, uint32_t c) noexcept
{
    if (a > b) std::swap(a, b);
    if (b > c) std::swap(b, c);
    if (a > b) std::swap(a, b);
    return (static_cast<uint64_t>(a) << 40) | (static_cast<uint64_t>(b) << 20) | static_cast<uint64_t>(c);
}

struct FaceKey {
    std::array<uint32_t, 3> v;
    bool operator==(const FaceKey& o) const noexcept { return v == o.v; }
};
struct FaceKeyHash {
    uint64_t operator()(const FaceKey& k) const noexcept {
        return hashTriangle(k.v[0], k.v[1], k.v[2]);
    }
};

// Exact-ish convex hull volume: fan the hull's own triangles against the origin through the
// orientation predicate, which differences the coordinates first (see the note on the 2D
// area helper — the shoelace/triple-product form on raw coordinates loses most of its digits
// on a sliver). Zero for a point set with no interior, which correctly has no
// tetrahedralization to compare against.
double convexHullVolume(const std::vector<Vec3>& points)
{
    const ConvexHull h = MeshConvexHull::build(points);
    if (h.faces.empty()) return 0.0;
    double six = 0.0;
    for (const auto& f : h.faces) {
        six += RobustPredicates::orient3D(h.vertices[f[0]], h.vertices[f[1]], h.vertices[f[2]],
                                          Vec3{0.f, 0.f, 0.f});
    }
    return std::abs(six) / 6.0;
}

double tetrahedraVolume(const TetMesh& m)
{
    double six = 0.0;
    for (const auto& t : m.tetrahedra) {
        six += std::abs(RobustPredicates::orient3D(m.vertices[t[0]], m.vertices[t[1]],
                                                   m.vertices[t[2]], m.vertices[t[3]]));
    }
    return six / 6.0;
}

TetMesh computeAtScale(const std::vector<Vec3>& points, float scaleMultiplier)
{
    TetMesh result;
    result.vertices = points;

    // Compute bounding box.
    Vec3 bmin = points[0], bmax = points[0];
    for (const auto& p : points) {
        bmin.x = std::min(bmin.x, p.x);
        bmin.y = std::min(bmin.y, p.y);
        bmin.z = std::min(bmin.z, p.z);
        bmax.x = std::max(bmax.x, p.x);
        bmax.y = std::max(bmax.y, p.y);
        bmax.z = std::max(bmax.z, p.z);
    }

    // Create super-tetrahedron vertices that enclose all points.
    float dx = (bmax.x - bmin.x) * 0.5f + 1.f;
    float dy = (bmax.y - bmin.y) * 0.5f + 1.f;
    float dz = (bmax.z - bmin.z) * 0.5f + 1.f;
    float cx = (bmin.x + bmax.x) * 0.5f;
    float cy = (bmin.y + bmax.y) * 0.5f;
    float cz = (bmin.z + bmax.z) * 0.5f;

    float scale = std::max({dx, dy, dz}) * scaleMultiplier;
    uint32_t sv0 = static_cast<uint32_t>(result.vertices.size());
    result.vertices.push_back({cx,           cy,           cz - scale});
    result.vertices.push_back({cx - scale,   cy + scale,   cz + scale});
    result.vertices.push_back({cx + scale,   cy + scale,   cz + scale});
    result.vertices.push_back({cx,           cy - scale,   cz + scale});
    const uint32_t sv1 = sv0 + 1;
    const uint32_t sv2 = sv0 + 2;
    const uint32_t sv3 = sv0 + 3;
    const uint32_t nPoints = static_cast<uint32_t>(points.size());

    // Start with the super-tetrahedron.
    std::vector<Tet> tets;
    Tet super;
    super.v = std::array<uint32_t, 4>{sv0, sv1, sv2, sv3};
    if (nonDegenerate(super.v, result.vertices)) {
        tets.push_back(super);
    }

    // Incremental Bowyer-Watson insertion.
    for (uint32_t pi = 0; pi < nPoints; ++pi) {
        const Vec3& p = points[pi];

        // Find all tetrahedra whose circumsphere contains p (cavity).
        std::vector<size_t> cavity;
        for (size_t ti = 0; ti < tets.size(); ++ti) {
            if (pointInCircumsphere(p, tets[ti].v, result.vertices)) {
                cavity.push_back(ti);
            }
        }

        if (cavity.empty()) continue;

        // Collect boundary triangles using FaceKey map.
        std::unordered_map<FaceKey, uint32_t, FaceKeyHash> faceCount;
        for (size_t idx : cavity) {
            const auto& v = tets[idx].v;
            std::array<std::array<uint32_t,3>, 4> faces = {{
                {{v[0], v[1], v[2]}}, {{v[0], v[1], v[3]}},
                {{v[0], v[2], v[3]}}, {{v[1], v[2], v[3]}}
            }};
            for (const auto& f : faces) {
                std::array<uint32_t,3> sf = f;
                if (sf[0] > sf[1]) std::swap(sf[0], sf[1]);
                if (sf[1] > sf[2]) std::swap(sf[1], sf[2]);
                if (sf[0] > sf[1]) std::swap(sf[0], sf[1]);
                faceCount[FaceKey{sf}]++;
            }
        }

        // Create new tetrahedra connecting p to each boundary face.
        for (const auto& [key, count] : faceCount) {
            if (count != 1) continue;
            Tet newTet;
            newTet.v = {pi, key.v[0], key.v[1], key.v[2]};
            if (nonDegenerate(newTet.v, result.vertices)) {
                tets.push_back(newTet);
            }
        }

        // Remove cavity tetrahedra (in reverse order to preserve indices).
        std::sort(cavity.begin(), cavity.end(), std::greater<size_t>());
        for (size_t idx : cavity) {
            tets[idx] = tets.back();
            tets.pop_back();
        }
    }

    // Remove tetrahedra that use super-tetrahedron vertices.
    std::vector<Tet> filtered;
    for (const auto& tet : tets) {
        bool usesSuper = false;
        for (int i = 0; i < 4; ++i) {
            if (tet.v[i] >= nPoints) { usesSuper = true; break; }
        }
        if (!usesSuper) filtered.push_back(tet);
    }

    // Also remove super-tetrahedron vertices.
    result.vertices.resize(nPoints);

    // Copy filtered tetrahedra to result.
    result.tetrahedra.reserve(filtered.size());
    for (const auto& tet : filtered) {
        result.tetrahedra.push_back(tet.v);
    }

    return result;
}

} // namespace

// A Delaunay tetrahedralization tiles the convex hull of its input exactly. Verify that, and
// grow the super-tetrahedron until it holds — the 3D case of the sizing problem documented in
// DelaunaySuperTriangle.h. A single fixed multiple of the bounding box cannot be right,
// because what the super-tetrahedron has to escape is the largest CIRCUMSPHERE in the
// triangulation, and that is driven by the input's thinnest tet rather than by its extent.
TetMesh TetDelaunay3D::compute(const std::vector<Vec3>& points,
                                const TetDelaunayOptions& opts) noexcept
{
    (void)opts;  // the exact in-sphere predicate needs no epsilon slop
    if (points.size() < 4) return TetMesh{};

    const double hullVolume = convexHullVolume(points);

    TetMesh best;
    double bestVolume = -1.0;
    for (const float scale : detail::kSuperTetScales) {
        TetMesh candidate = computeAtScale(points, scale);
        const double volume = tetrahedraVolume(candidate);
        if (volume > bestVolume) {
            bestVolume = volume;
            best = std::move(candidate);
        }
        // A coplanar/collinear input has no volume to cover, and returning nothing for it is
        // correct — do not spend the rest of the schedule on it.
        if (hullVolume <= 0.0 || detail::coversHull(bestVolume, hullVolume)) break;
    }
    return best;
}

} // namespace nexus::geometry
