#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Geometry — analytic B-rep boolean (select + emit)
//
//  Regularised boolean of two analytic solids, built on the boolean pipeline:
//  mutual imprint (segmentation, so no face straddles the other's boundary) →
//  per-face classification against the other solid → select the faces kept by
//  the operation → emit.
//
//  This first stage EMITS the result as one welded, watertight triangle Mesh
//  (the kept faces tessellated and their shared seam vertices merged). Upgrading
//  the output to a fully-sewn analytic Body (partnered coedges across the seam +
//  coplanar mergeFaces cleanup) and handling coincident/coplanar-overlap faces
//  are follow-up increments; this stage targets the clean case where the two
//  solids have no coincident faces (selection is purely Inside/Outside).
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/geometry/AnalyticBRep.h>
#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/Tolerance.h>

namespace nexus::geometry::brep {

enum class BooleanOp : uint8_t {
    Union,         // A ∪ B — keep A outside B + B outside A
    Intersection,  // A ∩ B — keep A inside B + B inside A
    Difference,    // A − B — keep A outside B + B inside A (B faces reversed)
};

// Regularised boolean of two solids, emitted as a welded triangle Mesh. Copies
// the inputs, mutually imprints them, selects the faces kept by `op` (via
// per-face classification against the other solid), tessellates them with
// outward-consistent winding (the kept B faces are reversed for Difference so
// the cavity boundary points outward), and welds coincident seam vertices so the
// result is watertight. Deterministic. Targets solids with no coincident faces.
[[nodiscard]] Mesh booleanToMesh(const Body& a, const Body& b, BooleanOp op,
                                 Tolerance tol = {});

// Regularised boolean of two solids, sewn into a fully analytic Body (not just a
// mesh). Same pipeline as booleanToMesh — copy, mutually imprint, select kept
// faces per op — but the kept analytic faces are re-assembled via Body::fromFaces
// (their seam vertices welded so A's and B's patches share edges, and the
// Difference-B faces reversed so orientation stays outward). The result is a
// first-class solid whose checkIntegrity/checkGeometry are clean and that can
// feed another boolean. Returns an empty Body if the kept faces do not sew into
// a valid solid. Deterministic. Targets solids with no coincident faces.
[[nodiscard]] Body booleanToBody(const Body& a, const Body& b, BooleanOp op,
                                 Tolerance tol = {});

}  // namespace nexus::geometry::brep
