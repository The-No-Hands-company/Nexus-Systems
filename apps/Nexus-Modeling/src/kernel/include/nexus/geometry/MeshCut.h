#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Geometry — Whole-mesh cut along the mutual intersection curve
//
//  Given two meshes A and B, retriangulate both so that no face straddles the
//  other surface: every triangle that the two surfaces cross is split along the
//  exact intersection segments (TriTriIntersect + TriangleRetriangulate). The
//  result surfaces are identical to the inputs (same geometry / area) — only the
//  tessellation changes, with new edges lying exactly on the intersection curve.
//
//  This is boolean-rebuild increment 3: the cut meshes are what the next stage
//  classifies (inside/outside) and stitches, giving a clean CSG result on coarse
//  meshes instead of the current whole-triangle centroid classification.
//
//  Broad phase is AABB brute-force (correct; a MeshBVH broad phase is a later
//  scalability optimization). Coplanar-overlap faces are left un-split for now.
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/geometry/Mesh.h>

namespace nexus::geometry {

struct MeshCutResult {
    Mesh a;  // A retriangulated along the A∩B curve
    Mesh b;  // B retriangulated along the A∩B curve
};

class MeshCut {
public:
    [[nodiscard]] static MeshCutResult cut(const Mesh& a, const Mesh& b) noexcept;
};

} // namespace nexus::geometry
