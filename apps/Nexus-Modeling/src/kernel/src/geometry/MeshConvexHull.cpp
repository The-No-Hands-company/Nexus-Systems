#include <nexus/geometry/MeshConvexHull.h>

#include <nexus/geometry/RobustPredicates.h>
#include <nexus/geometry/Tolerance.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

namespace {

// Use bit_cast to avoid -ffast-math silently optimising NaN checks away.
[[nodiscard]] bool isFiniteVec3(const Vec3& v) noexcept {
    const std::uint32_t bx = std::bit_cast<std::uint32_t>(v.x);
    const std::uint32_t by = std::bit_cast<std::uint32_t>(v.y);
    const std::uint32_t bz = std::bit_cast<std::uint32_t>(v.z);
    return (bx & 0x7F800000u) != 0x7F800000u &&
           (by & 0x7F800000u) != 0x7F800000u &&
           (bz & 0x7F800000u) != 0x7F800000u;
}

// A face of the hull in progress, wound counter-clockwise as seen from OUTSIDE, so that
// (b-a) x (c-a) is the outward normal. Every operation below preserves that winding; it is
// never re-derived from a "flip towards the centre" guess, because a growing hull has no
// point that is reliably interior.
struct HullFace {
    uint32_t a = 0, b = 0, c = 0;
    bool removed = false;
};

[[nodiscard]] constexpr uint64_t edgeKey(uint32_t u, uint32_t v) noexcept {
    return (static_cast<uint64_t>(u) << 32) | static_cast<uint64_t>(v);
}

// Exact collinearity: three points are collinear iff all three axis-plane projections have
// zero signed area. orient2D is exact, so this is a decision and not an estimate.
[[nodiscard]] bool collinear(const Vec3d& p, const Vec3d& q, const Vec3d& r) noexcept {
    return RobustPredicates::orient2D({p.x, p.y}, {q.x, q.y}, {r.x, r.y}) == 0.0 &&
           RobustPredicates::orient2D({p.y, p.z}, {q.y, q.z}, {r.y, r.z}) == 0.0 &&
           RobustPredicates::orient2D({p.z, p.x}, {q.z, q.x}, {r.z, r.x}) == 0.0;
}

} // namespace

ConvexHull MeshConvexHull::build(const std::vector<Vec3>& points) {
    ConvexHull result;

    // Drop non-finite points, then drop exact duplicates: a repeated point is visible from
    // nowhere and would only add degenerate work. Insertion order of the survivors is kept
    // so the output is reproducible for a given input (the determinism contract).
    std::vector<Vec3> pts;
    pts.reserve(points.size());
    {
        std::unordered_map<uint64_t, std::vector<uint32_t>> seen;
        for (const auto& p : points) {
            if (!isFiniteVec3(p)) continue;
            const uint64_t h = (static_cast<uint64_t>(std::bit_cast<std::uint32_t>(p.x)) * 0x9E3779B97F4A7C15ull) ^
                               (static_cast<uint64_t>(std::bit_cast<std::uint32_t>(p.y)) * 0xC2B2AE3D27D4EB4Full) ^
                               (static_cast<uint64_t>(std::bit_cast<std::uint32_t>(p.z)) * 0x165667B19E3779F9ull);
            auto& bucket = seen[h];
            bool dup = false;
            for (uint32_t idx : bucket) {
                if (pts[idx].x == p.x && pts[idx].y == p.y && pts[idx].z == p.z) { dup = true; break; }
            }
            if (dup) continue;
            bucket.push_back(static_cast<uint32_t>(pts.size()));
            pts.push_back(p);
        }
    }
    if (pts.size() < 4) return result;

    const uint32_t n = static_cast<uint32_t>(pts.size());
    std::vector<Vec3d> q(n);
    for (uint32_t i = 0; i < n; ++i) q[i] = Vec3d{pts[i]};

    // ── Seed tetrahedron ──────────────────────────────────────────────────────────────
    // The lexicographic extremes are hull vertices for certain, and picking them by a total
    // order (rather than by a per-axis scan that can tie) keeps the seed deterministic.
    auto lexLess = [&](uint32_t i, uint32_t j) {
        if (q[i].x != q[j].x) return q[i].x < q[j].x;
        if (q[i].y != q[j].y) return q[i].y < q[j].y;
        return q[i].z < q[j].z;
    };
    uint32_t s0 = 0, s1 = 0;
    for (uint32_t i = 1; i < n; ++i) {
        if (lexLess(i, s0)) s0 = i;
        if (lexLess(s1, i)) s1 = i;
    }

    uint32_t s2 = UINT32_MAX;
    for (uint32_t i = 0; i < n; ++i) {
        if (i == s0 || i == s1) continue;
        if (!collinear(q[s0], q[s1], q[i])) { s2 = i; break; }
    }
    if (s2 == UINT32_MAX) return result; // every point on one line

    uint32_t s3 = UINT32_MAX;
    for (uint32_t i = 0; i < n; ++i) {
        if (i == s0 || i == s1 || i == s2) continue;
        if (RobustPredicates::orient3D(q[s0], q[s1], q[s2], q[i]) != 0.0) { s3 = i; break; }
    }
    if (s3 == UINT32_MAX) return result; // every point on one plane

    std::vector<HullFace> faces;
    std::unordered_map<uint64_t, uint32_t> edgeFace; // directed edge -> face index

    bool consistent = true;
    auto linkFace = [&](uint32_t fi) {
        const HullFace& f = faces[fi];
        const uint32_t v[3] = {f.a, f.b, f.c};
        for (int e = 0; e < 3; ++e) {
            if (!edgeFace.emplace(edgeKey(v[e], v[(e + 1) % 3]), fi).second) consistent = false;
        }
    };
    auto unlinkFace = [&](uint32_t fi) {
        const HullFace& f = faces[fi];
        const uint32_t v[3] = {f.a, f.b, f.c};
        for (int e = 0; e < 3; ++e) edgeFace.erase(edgeKey(v[e], v[(e + 1) % 3]));
    };
    // Add (x,y,z) wound so its outward normal points AWAY from the opposite vertex `w`.
    auto addOriented = [&](uint32_t x, uint32_t y, uint32_t z, uint32_t w) {
        if (RobustPredicates::orient3D(q[x], q[y], q[z], q[w]) < 0.0) std::swap(y, z);
        faces.push_back(HullFace{x, y, z, false});
        linkFace(static_cast<uint32_t>(faces.size() - 1));
    };

    addOriented(s0, s1, s2, s3);
    addOriented(s0, s1, s3, s2);
    addOriented(s0, s2, s3, s1);
    addOriented(s1, s2, s3, s0);
    if (!consistent) return result;

    // ── Incremental insertion ─────────────────────────────────────────────────────────
    std::vector<uint8_t> visible;
    std::vector<uint32_t> visibleList;
    std::vector<std::pair<uint32_t, uint32_t>> horizon;

    for (uint32_t p = 0; p < n; ++p) {
        if (p == s0 || p == s1 || p == s2 || p == s3) continue;

        // A face sees p iff p is strictly on the far side of its outward normal. This is an
        // exact sign, so a point that is inside or exactly ON the hull is seen by nothing and
        // is correctly skipped; no visibility epsilon is involved anywhere.
        visible.assign(faces.size(), 0);
        visibleList.clear();
        for (uint32_t fi = 0; fi < faces.size(); ++fi) {
            if (faces[fi].removed) continue;
            const HullFace& f = faces[fi];
            if (RobustPredicates::orient3D(q[f.a], q[f.b], q[f.c], q[p]) < 0.0) {
                visible[fi] = 1;
                visibleList.push_back(fi);
            }
        }
        if (visibleList.empty()) continue; // p is inside the hull, or on its boundary

        // The horizon is the set of DIRECTED edges whose face is visible and whose twin's is
        // not. Keeping the direction is what carries the winding onto the new faces: a new
        // face reuses the horizon edge as-is, so it is wound exactly as the face it replaces.
        horizon.clear();
        for (uint32_t fi : visibleList) {
            const HullFace& f = faces[fi];
            const uint32_t v[3] = {f.a, f.b, f.c};
            for (int e = 0; e < 3; ++e) {
                const uint32_t u = v[e], w = v[(e + 1) % 3];
                auto it = edgeFace.find(edgeKey(w, u)); // the twin, traversed the other way
                if (it == edgeFace.end()) { consistent = false; break; }
                if (!visible[it->second]) horizon.emplace_back(u, w);
            }
            if (!consistent) break;
        }
        if (!consistent || horizon.empty()) return {};

        for (uint32_t fi : visibleList) { unlinkFace(fi); faces[fi].removed = true; }

        for (const auto& [u, w] : horizon) {
            faces.push_back(HullFace{u, w, p, false});
            linkFace(static_cast<uint32_t>(faces.size() - 1));
        }
        // A hull that has stopped being a closed orientable surface cannot be repaired by
        // continuing; report nothing rather than something that only looks like a hull.
        if (!consistent) return {};
    }

    // Every directed edge must have its twin, or the surface is not closed.
    for (const auto& [key, fi] : edgeFace) {
        if (faces[fi].removed) continue;
        const uint32_t u = static_cast<uint32_t>(key >> 32);
        const uint32_t w = static_cast<uint32_t>(key & 0xFFFFFFFFull);
        if (!edgeFace.count(edgeKey(w, u))) return {};
    }

    std::unordered_map<uint32_t, uint32_t> vMap;
    for (const auto& f : faces) {
        if (f.removed) continue;
        std::array<uint32_t, 3> tri{};
        const uint32_t v[3] = {f.a, f.b, f.c};
        for (int k = 0; k < 3; ++k) {
            auto [it, inserted] = vMap.emplace(v[k], static_cast<uint32_t>(result.vertices.size()));
            if (inserted) result.vertices.push_back(pts[v[k]]);
            tri[static_cast<size_t>(k)] = it->second;
        }
        result.faces.push_back(tri);
    }

    return result;
}

ConvexHull MeshConvexHull::fromMesh(const Mesh& mesh) {
    if (!mesh.isValid()) return {};
    return build(mesh.attributes().positions());
}

} // namespace nexus::geometry
