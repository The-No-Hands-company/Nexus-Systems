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

// Flat (45°) chamfer of one axis-aligned edge of a box, as a boolean difference
// with a triangular cutting prism (a right-triangle profile of legs = setback,
// extruded along the edge). `axis` (0=X, 1=Y, 2=Z) is the edge's direction;
// `s1`/`s2` (each −1 or +1) pick the edge by the sign of the two OTHER box
// coordinates (so a box has 12 edges: 4 per axis). `setback` is the leg length,
// clamped to the box; a non-positive setback returns the box unchanged. The
// removed wedge has volume ½·setback²·edgeLength. Returns a watertight solid.
[[nodiscard]] Body chamferBoxEdge(const Body& box, int axis, int s1, int s2, float setback);

// Rounded (fillet) of one axis-aligned box edge, as a boolean difference with a
// cutting prism whose cross-section is the corner minus a quarter-disk of the
// given `radius` (its arc faceted into `segments` chords, so the tool is
// all-planar). Same edge selector/clamping as chamferBoxEdge; radius ≤ 0 returns
// the box. The removed wedge → radius²·(1 − π/4)·edgeLength as segments grow.
[[nodiscard]] Body filletBoxEdge(const Body& box, int axis, int s1, int s2, float radius,
                                 uint32_t segments = 8);

// Hollow (shell) an axis-aligned box centred at the origin: subtract a concentric
// inner box, each dimension inset by 2·`thickness`, via a boolean difference —
// leaving a SEALED wall of the given thickness. The result is a valid solid with
// TWO boundary shells (outer surface + inner cavity, the cavity faces pointing
// inward) → euler 4, watertight (boundaryEdges 0), material volume =
// w·h·d − (w−2t)(h−2t)(d−2t). If 2·thickness ≥ the smallest dimension (no room
// for a cavity) the plain solid box is returned; non-positive/non-finite
// thickness or dimensions likewise return the plain box (or empty for a
// degenerate box). Deterministic. (An OPEN container — a removed top face — is a
// follow-up.)
[[nodiscard]] Body hollowBox(float width, float height, float depth, float thickness);

}  // namespace nexus::geometry::brep
