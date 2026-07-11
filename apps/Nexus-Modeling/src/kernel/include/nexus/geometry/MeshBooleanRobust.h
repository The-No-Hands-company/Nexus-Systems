#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Geometry — Robust mesh boolean (splits along the intersection curve)
//
//  The real CSG pipeline, assembled from the foundation primitives:
//    MeshCut (retriangulate A,B along A∩B) → classify each sub-triangle
//    inside/outside the other mesh → keep per operation → weld the seam.
//
//  Unlike the legacy whole-triangle centroid classifier, this produces a clean
//  watertight result on COARSE meshes (the seam follows the true intersection
//  curve). Coplanar-overlap faces are a known limitation (handled by a later
//  increment); inputs are triangulated internally.
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/geometry/BooleanOperation.h>  // BooleanOperationType
#include <nexus/geometry/Mesh.h>

namespace nexus::geometry {

// Returns the boolean result mesh (welded). On non-finite / empty input returns
// an empty mesh.
[[nodiscard]] Mesh robustMeshBoolean(const Mesh& a, const Mesh& b, BooleanOperationType op) noexcept;

} // namespace nexus::geometry
