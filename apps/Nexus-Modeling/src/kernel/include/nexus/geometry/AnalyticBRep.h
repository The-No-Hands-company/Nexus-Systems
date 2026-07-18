#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Geometry — Analytic Boundary Representation (analytic B-rep)
//
//  A CAD-grade solid model: typed topology (vertex / edge / coedge / loop /
//  face / shell / solid) bound to ANALYTIC geometry (exact curves & surfaces),
//  as opposed to the mesh-backed `BRepBody` whose faces are polygons. This is
//  the foundation the exact-solid CAD stack sits on.
//
//  Topology model (OpenCASCADE / ACIS-style, oriented use-based):
//    • Vertex  — an exact point.
//    • Edge    — a Curve segment [t0,t1] between two vertices (geometry shared
//                by the two faces that meet at it).
//    • Coedge  — an ORIENTED use of an edge by one loop (the analog of a
//                half-edge); its `partner` is the coedge on the adjacent face.
//    • Loop    — a closed ring of coedges bounding a face (one outer + N inner).
//    • Face    — a Surface trimmed by its loops, with an orientation flag.
//    • Shell   — a connected set of faces (closed ⇒ watertight solid boundary).
//    • Solid   — one outer shell plus optional inner (void) shells.
//
//  Geometry starts with the analytic primitives needed for planar/quadric
//  solids (Line / Circle curves, Plane / Cylinder / Sphere surfaces) and a
//  handle slot for wiring the existing NurbsCurve / NurbsSurface toolkit in a
//  later increment. All numeric predicates use bit-inspection finite checks and
//  the central Tolerance module (`-ffast-math` safe).
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/MeshMassProperties.h>
#include <nexus/geometry/NurbsSurface.h>
#include <nexus/geometry/Tolerance.h>
#include <nexus/render/Camera.h>

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace nexus::geometry::brep {

using nexus::render::Vec3;

inline constexpr uint32_t kInvalid = 0xFFFFFFFFu;

// ──────────── Analytic geometry ──────────────────────────────────────────────

enum class CurveKind : uint8_t { Line, Circle, Nurbs };

struct Curve {
    CurveKind kind = CurveKind::Line;
    Vec3  origin{};                 // Line: point on line;   Circle: centre
    Vec3  dir{1.f, 0.f, 0.f};       // Line: unit direction;  Circle: axis normal
    Vec3  ref{0.f, 1.f, 0.f};       // Circle: unit radius direction at t = 0
    float radius = 0.f;             // Circle radius
    uint32_t nurbs = kInvalid;      // index into a NurbsCurve store (future)

    // Point at parameter t (Line: origin + t*dir; Circle: swept by angle t).
    [[nodiscard]] Vec3 eval(float t) const noexcept;
    // Unit tangent direction at parameter t.
    [[nodiscard]] Vec3 normalAt(float t) const noexcept;
};

enum class SurfaceKind : uint8_t { Plane, Cylinder, Sphere, Nurbs };

struct Surface {
    SurfaceKind kind = SurfaceKind::Plane;
    Vec3  origin{};                 // point on surface / centre / axis base
    Vec3  normal{0.f, 0.f, 1.f};    // Plane normal; Cylinder & Sphere axis
    Vec3  uAxis{1.f, 0.f, 0.f};     // in-surface u direction (param frame)
    float radius = 0.f;             // Cylinder / Sphere radius
    uint32_t nurbs = kInvalid;      // index into a NurbsSurface store (future)

    [[nodiscard]] Vec3 eval(float u, float v) const noexcept;
    [[nodiscard]] Vec3 normalAt(float u, float v) const noexcept;
    // v-axis = normal × uAxis (right-handed param frame).
    [[nodiscard]] Vec3 vAxis() const noexcept;
};

// ──────────── Topology entities ──────────────────────────────────────────────

struct Vertex {
    Vec3     point{};
    uint32_t coedge = kInvalid;     // one coedge starting here
    bool     alive = true;          // tombstone flag (dead entities are skipped)
};

struct Edge {
    uint32_t curve = kInvalid;
    uint32_t v0 = kInvalid, v1 = kInvalid;   // curve runs v0 → v1 over [t0,t1]
    float    t0 = 0.f, t1 = 1.f;
    uint32_t coedge = kInvalid;     // one coedge using this edge
    bool     alive = true;          // tombstone flag (dead entities are skipped)
};

// A parameter-space trim curve (pcurve): the 2D image of a coedge in the (u,v)
// parameter domain of its owning face's surface. On a trimmed surface — the
// point of a NURBS B-rep face — the trim boundary is defined ON the parameter
// domain, and surfacePoint maps a pcurve endpoint back to the coedge's 3D
// vertex. A pcurve is stored per-COEDGE, not per-edge, because the two faces
// sharing an edge have distinct parameter domains (and thus distinct pcurves).
//
// A pcurve is a POLYLINE start → interior… → end in (u,v): with no interior
// points it is a straight segment (the parameter-space analog of a Line); with
// interior points it approximates a CURVED trim (an arc is a sampled polyline),
// so a NURBS face can be bounded by curved trims that follow the surface.
struct Pcurve {
    bool  present = false;
    float u0 = 0.f, v0 = 0.f;       // param-space start (maps to the coedge start vertex)
    float u1 = 0.f, v1 = 0.f;       // param-space end   (maps to the coedge end vertex)
    std::vector<std::pair<float, float>> interior;  // (u,v) points strictly between start & end
};

struct Coedge {
    uint32_t edge = kInvalid;
    bool     reversed = false;      // true ⇒ traverses the edge v1 → v0
    uint32_t loop = kInvalid;
    uint32_t next = kInvalid, prev = kInvalid;  // around the loop
    uint32_t partner = kInvalid;    // the coedge on the adjacent face (same edge)
    Pcurve   pcurve;                // optional parameter-space trim curve on the face
    bool     alive = true;          // tombstone flag (dead entities are skipped)
};

struct Loop {
    uint32_t face = kInvalid;
    uint32_t first = kInvalid;      // a coedge in the ring
    bool     outer = true;          // outer boundary vs inner hole
    bool     alive = true;          // tombstone flag (dead entities are skipped)
};

struct Face {
    uint32_t surface = kInvalid;
    bool     reversed = false;      // surface normal flipped vs face outward normal
    uint32_t outerLoop = kInvalid;
    std::vector<uint32_t> innerLoops;
    uint32_t shell = kInvalid;
    bool     alive = true;          // tombstone flag (dead entities are skipped)
};

struct Shell {
    std::vector<uint32_t> faces;
    bool closed = false;
};

struct Solid {
    std::vector<uint32_t> shells;   // [0] = outer, rest = inner voids
};

// ──────────── Body ───────────────────────────────────────────────────────────

class Body {
public:
    // Result of the integrity validator (counts are of live entities).
    struct IntegrityReport {
        bool ok = true;
        std::string reason;         // first violation; empty when ok
        uint32_t vertices = 0, edges = 0, faces = 0;
        uint32_t coedges = 0, loops = 0, shells = 0;
        uint32_t boundaryEdges = 0; // edges used by only one coedge (open shell)
        int euler = 0;              // V - E + F
    };

    // Result of the geometric-consistency validator: the analytic geometry
    // agrees with the topology (as opposed to checkIntegrity, which validates
    // only the topology). Both must hold; ops must preserve both.
    struct GeometryReport {
        bool ok = true;
        std::string reason;        // first violation; empty when ok
        uint32_t checkedEdges = 0;
    };

    // One face definition for fromFaces(): a CCW vertex-index ring + its surface.
    // If nurbsSurface is set, the face lies on that exact NURBS surface (stored
    // in the Body); `surface` is then tagged Nurbs with a handle into the store.
    struct FaceDef {
        std::vector<uint32_t> loop;
        Surface surface;
        std::optional<NurbsSurface> nurbsSurface;
    };

    // Assemble a body from a point set and per-face vertex rings. Edges are
    // deduplicated across faces and coedges partnered (the analytic analog of
    // HalfEdgeMesh::fromMesh). Returns nullopt on malformed input.
    [[nodiscard]] static std::optional<Body> fromFaces(
        const std::vector<Vec3>& points, const std::vector<FaceDef>& faces);

    // Direct half-edge/analytic-invariant validation over all entities.
    [[nodiscard]] IntegrityReport checkIntegrity() const;

    // Euler operator — split an edge at parameter t in (0,1): inserts a vertex
    // on the edge's curve, replacing the edge with two edges sharing the curve
    // over restricted param ranges, and splitting the coedge(s) on the incident
    // face(s) accordingly. χ-neutral (ΔV=+1, ΔE=+1, ΔF=0). Returns the new
    // vertex id, or kInvalid on failure. Preserves checkIntegrity + checkGeometry.
    uint32_t splitEdge(uint32_t edgeId, float t);

    // Euler operator — split a face by a new edge (a Line) between two
    // NON-ADJACENT vertices of its outer loop, producing two faces that inherit
    // the original surface. χ-neutral (ΔV=0, ΔE=+1, ΔF=+1). Returns the new
    // face id, or kInvalid on failure. Preserves checkIntegrity + checkGeometry.
    uint32_t splitFace(uint32_t faceId, uint32_t vA, uint32_t vB);

    // The vertices of a face's outer loop, in order (for queries / editing).
    [[nodiscard]] std::vector<uint32_t> faceVertices(uint32_t faceId) const;

    // Imprint an analytic intersection curve onto a planar face (the B-rep
    // boolean's imprint step). The curve — a Line from intersectSurfaces(), e.g.
    // plane∩plane — must pierce the face's outer boundary in exactly two interior
    // points on Line boundary edges. A vertex is introduced at each piercing
    // (reusing splitEdge), then the face is cut between them by a NEW edge that
    // carries `curve` itself (not a straight chord), so the shared edge lies
    // exactly on the intersection curve and, being a Line in the face's plane,
    // on the face. χ-neutral (ΔV=+2, ΔE=+3, ΔF=+1). Returns the new face id, or
    // kInvalid if the curve does not cross the boundary in exactly two interior
    // points. Preserves both validators. (Circle imprint — the coplanar in-face
    // "bite" and cuts on curved faces — is a dedicated follow-up increment.)
    uint32_t imprintCurve(uint32_t faceId, const Curve& curve, Tolerance tol = {});

    // Reclassify an edge's curve as a Circle arc about `center`/`axis` at
    // `radius` (the endpoints must lie on that circle). Its param range is set so
    // the curve still reproduces the endpoint vertices — so checkGeometry holds
    // and toMesh(subdivisions) now tessellates it smoothly. Returns false if the
    // endpoints are not on the circle within tolerance.
    bool setEdgeArc(uint32_t edgeId, const Vec3& center, const Vec3& axis, float radius,
                    Tolerance tol = {});

    // Attach a parameter-space trim curve (pcurve) to a coedge: a straight
    // segment in the owning face's surface (u,v) domain from (u0,v0) to (u1,v1),
    // running the coedge's directed start → end. This is what makes a NURBS face
    // a *trimmed* surface — the boundary is defined on the parameter domain, not
    // just in 3D. Validated on attach (and re-checked by checkGeometry):
    // surfacePoint(face surface, u,v) at each endpoint must reproduce the
    // coedge's directed 3D endpoint vertices within tol. Returns false — leaving
    // the coedge unchanged — on invalid ids, a non-finite parameter, or endpoints
    // that do not map back to the coedge's vertices.
    bool setCoedgePcurve(uint32_t coedgeId, float u0, float v0, float u1, float v1,
                         Tolerance tol = {});

    // Attach a CURVED (polyline) parameter-space trim curve to a coedge: the
    // (u,v) points run the coedge's directed start → end, with `points.front()`
    // mapping to the start vertex and `points.back()` to the end vertex (all
    // validated by surfacePoint within tol, as setCoedgePcurve). Interior points
    // approximate a curved trim (an arc sampled into a polyline); they must be
    // finite and inside the surface's parameter domain. Requires ≥2 points.
    // Returns false — leaving the coedge unchanged — on too few points, a
    // non-finite / out-of-domain point, or endpoints that do not map back.
    bool setCoedgePcurvePolyline(uint32_t coedgeId,
                                 const std::vector<std::pair<float, float>>& points,
                                 Tolerance tol = {});

    // Euler operator (inverse of splitEdge / kill-edge-vertex) — remove a
    // degree-2 vertex whose two incident edges share a curve, merging them into
    // one edge (dead entities are tombstoned + unlinked). χ-neutral
    // (ΔV=-1, ΔE=-1, ΔF=0). Returns true on success. Round-trips with splitEdge.
    bool joinEdges(uint32_t vertexId);

    // Euler operator (inverse of splitFace / kill-edge-face) — remove the shared
    // edge between two adjacent faces, splicing their loops into one and merging
    // the faces (tombstoning the removed edge/coedges/face/loop). χ-neutral
    // (ΔV=0, ΔE=-1, ΔF=-1). Returns true on success. Round-trips with splitFace.
    bool mergeFaces(uint32_t edgeId);

    // Cleanup: merge adjacent COPLANAR faces (planes with equal outward normal
    // and the same offset, within tol) that share EXACTLY ONE edge — via the
    // mergeFaces Euler op — iterating to a fixpoint. Collapses the coplanar
    // over-segmentation left by imprint / boolean without changing the solid
    // (χ-neutral, volume-preserving; euler and closedness are unchanged). Pairs
    // sharing 2+ edges are left (their internal edge would become a slit); the
    // collinear-edge / degree-2-vertex cleanup that unlocks those is a separate
    // pass. Returns the number of merges performed. Deterministic.
    uint32_t mergeCoplanarFaces(Tolerance tol = {});

    // Cleanup: remove redundant degree-2 vertices whose two incident edges are
    // COLLINEAR Lines (same supporting line within tol) — merging them into one
    // edge (the general kill-edge-vertex for collinear separate-curve Lines, the
    // counterpart of mergeCoplanarFaces for edges). Iterates to a fixpoint;
    // χ-neutral, volume-preserving. Returns the number of vertices removed.
    uint32_t mergeCollinearEdges(Tolerance tol = {});

    // Cleanup to a minimal B-rep: alternate mergeCoplanarFaces + mergeCollinearEdges
    // to a combined fixpoint (each unlocks the other — merging collinear edges
    // exposes new single-shared-edge coplanar pairs, and vice versa). Preserves
    // the solid (validators clean, euler/closedness/volume unchanged). Returns the
    // total number of merge operations performed.
    uint32_t simplify(Tolerance tol = {});

    // Validates that the analytic geometry is consistent with the topology:
    // every edge's curve reproduces its endpoint vertices over its param range,
    // all geometry is finite, surface normals are unit length, and partnered
    // coedges agree on the shared edge's endpoints. Uses the central Tolerance.
    [[nodiscard]] GeometryReport checkGeometry(Tolerance tol = {}) const;

    // Versioned binary serialization of the full analytic B-rep (topology +
    // analytic geometry + the NURBS-surface store). Deterministic byte output.
    // deserialize returns nullopt on a bad magic/version, a truncated/garbage
    // buffer, or a non-finite float in the stream (bit-inspection checked). The
    // format is forward-versioned: a reader accepts its own and earlier versions.
    [[nodiscard]] std::vector<std::uint8_t> serialize() const;
    [[nodiscard]] static std::optional<Body> deserialize(const std::vector<std::uint8_t>& bytes);

    // Tessellation of the shell to a triangle mesh. `subdivisions` intermediate
    // points are placed on each EDGE via its curve — shared by both incident
    // faces, so the result is watertight (crack-free) at any level; curved edges
    // (Circle / NURBS) therefore tessellate smoothly. subdivisions=0 is the flat
    // control-vertex tessellation. Dead entities are skipped.
    [[nodiscard]] Mesh toMesh(uint32_t subdivisions = 0) const;

    // Tessellate a SINGLE trimmed NURBS face by walking its parameter-space trim
    // curves (pcurves), not its 3D loop. The face must be on a NURBS surface and
    // its outer loop (and any inner loops) must be fully pcurve-trimmed. The trim
    // loops are assembled in (u,v) space, the surface is grid-sampled across its
    // parameter domain at `gridRes`×`gridRes`, and a cell's two triangles are
    // emitted — with vertices evaluated ON the NURBS surface — when the cell's
    // CENTRE lies inside the trim region (even-odd across all loops; centre
    // classification is robust even when the trim boundary is grid-aligned). So
    // every emitted vertex lies exactly on the surface, and the tessellated area
    // converges to the true trimmed area (exact when the trim boundary lands on
    // grid lines). Returns an empty Mesh if the face is not a fully-pcurve-trimmed
    // NURBS face. Deterministic.
    [[nodiscard]] Mesh tessellateTrimmedFace(uint32_t faceId, uint32_t gridRes,
                                             Tolerance tol = {}) const;

    // Add an INNER trim loop (a hole) to a face: builds a ring of new vertices at
    // `ringPoints` (which should lie on the face's surface), Line edges between
    // them, and a coedge ring marked as an inner loop, appended to the face's
    // innerLoops. The hole boundary is unpartnered (a hole punched in the surface,
    // not a passage through a solid). The caller then attaches a pcurve to each
    // returned inner coedge so tessellateTrimmedFace excludes the hole in (u,v).
    // Returns the first coedge id of the new inner loop, or kInvalid on failure
    // (bad face id, <3 points, non-finite point). Preserves checkIntegrity.
    uint32_t addTrimHole(uint32_t faceId, const std::vector<Vec3>& ringPoints);

    // Classification of a point against the solid (the B-rep boolean's classify
    // step): Inside / Outside the shell, or OnBoundary (on a face within tol).
    enum class PointContainment : uint8_t { Outside, Inside, OnBoundary };

    // Classify a point against the (closed) solid boundary. Casts a ray from p
    // and counts crossings of the watertight, crack-free triangulation of the
    // shell (odd ⇒ Inside); an irrational ray direction with degeneracy-retry
    // keeps it robust to grazing hits, and the result is deterministic.
    // OnBoundary is reported when p lies within tol of the tessellated boundary
    // — exact for planar-faced solids (box / prisms / caps) and to tessellation
    // resolution for curved faces, which is the consistent semantics the boolean
    // needs when classifying tessellated sub-face centroids. Returns Outside for
    // a body with no faces.
    [[nodiscard]] PointContainment classifyPoint(const Vec3& p, Tolerance tol = {}) const;

    // A representative interior point of a face — the average of its outer-loop
    // vertices. For a planar face this centroid lies exactly on the face (and,
    // for the convex faces the boolean produces, inside it); it is the sample
    // point used to classify the face against another solid.
    [[nodiscard]] Vec3 faceCentroid(uint32_t faceId) const;

    // Classify a whole face against another solid, via its centroid: Inside /
    // Outside `other`, or OnBoundary when the centroid lies on other's boundary
    // (coincident faces). This is the boolean's per-face keep/discard decision
    // (imprint → classify each sub-face against the OTHER solid → select per op →
    // sew). Returns Outside for a dead/invalid face.
    [[nodiscard]] PointContainment classifyFace(uint32_t faceId, const Body& other,
                                                Tolerance tol = {}) const;

    // EXACT side of a point relative to a face's supporting plane, using the
    // Shewchuk adaptive-exact orient3D predicate (float fast path → exact
    // fallback) rather than a plain-float signed distance that can flip sign
    // under catastrophic cancellation. Returns +1 when p is on the OUTWARD-normal
    // side, -1 on the inward side, and 0 when p lies within `tol` of the plane
    // (a genuine on-boundary band). This is the robust core of the boolean's
    // which-side / face-straddle / coplanar decisions. Returns 0 for a
    // dead/invalid/degenerate face.
    [[nodiscard]] int facePlaneSide(uint32_t faceId, const Vec3& p, Tolerance tol = {}) const;

    // Apply an affine transform to the WHOLE body — vertex points AND every
    // analytic Curve (Line/Circle) and Surface (Plane/Cylinder/Sphere) frame —
    // so both validators stay clean afterward (moving only the vertices would
    // leave the curves/surfaces stale and corrupt toMesh). The linear part must
    // be a proper rotation optionally times a UNIFORM scale (radii and Line
    // param ranges scale by that factor; Circle angles are preserved); shear,
    // non-uniform scale and reflections are not representable on the analytic
    // geometry. Returns false — leaving the body unchanged — for a non-finite
    // matrix, an unsupported linear part, or a body with NURBS-backed faces (a
    // follow-up). Deterministic.
    bool transform(const nexus::render::Mat4& m);

    // Pure-translation shortcut for transform().
    bool translate(const Vec3& t);

    // Mass properties of the solid — volume, centroid, and the inertia tensor
    // about the centroid — integrated over the watertight boundary tessellation
    // via the divergence theorem (delegated to MeshMassProperties). `density`
    // scales the inertia (mass = density·volume; volume and centroid are the
    // geometric ones). Meaningful only for a closed solid; an open/degenerate
    // body yields a zero result.
    [[nodiscard]] nexus::geometry::MassProperties massProperties(float density = 1.f) const;

    // Total boundary surface area of the solid, summed over its watertight
    // triangulation (curved arc edges subdivided so cylinder/sphere converge,
    // planar faces exact). Deterministic.
    [[nodiscard]] float surfaceArea() const;

    // Evaluates a face's surface at (u,v), dispatching to the stored NURBS
    // surface when the face is on one, else to the analytic Surface::eval.
    [[nodiscard]] Vec3 surfacePoint(uint32_t surfaceId, float u, float v) const;
    [[nodiscard]] size_t nurbsSurfaceCount() const noexcept { return m_nurbsSurfaces.size(); }

    [[nodiscard]] size_t vertexCount() const noexcept { return m_verts.size(); }
    [[nodiscard]] size_t edgeCount()   const noexcept { return m_edges.size(); }
    [[nodiscard]] size_t coedgeCount() const noexcept { return m_coedges.size(); }
    [[nodiscard]] size_t loopCount()   const noexcept { return m_loops.size(); }
    [[nodiscard]] size_t faceCount()   const noexcept { return m_faces.size(); }
    [[nodiscard]] size_t shellCount()  const noexcept { return m_shells.size(); }
    [[nodiscard]] size_t solidCount()  const noexcept { return m_solids.size(); }
    [[nodiscard]] size_t surfaceCount() const noexcept { return m_surfaces.size(); }

    [[nodiscard]] const Vertex&  vertex(uint32_t i) const { return m_verts[i]; }
    // Mutable vertex access (for editing ops; checkGeometry guards consistency
    // after any geometry change).
    [[nodiscard]] Vertex& vertexMut(uint32_t i) { return m_verts[i]; }
    [[nodiscard]] Face& faceMut(uint32_t i) { return m_faces[i]; }
    [[nodiscard]] const Edge&    edge(uint32_t i)   const { return m_edges[i]; }
    [[nodiscard]] const Coedge&  coedge(uint32_t i) const { return m_coedges[i]; }
    [[nodiscard]] const Loop&    loop(uint32_t i)   const { return m_loops[i]; }
    [[nodiscard]] const Face&    face(uint32_t i)   const { return m_faces[i]; }
    [[nodiscard]] const Shell&   shell(uint32_t i)  const { return m_shells[i]; }
    [[nodiscard]] const Curve&   curve(uint32_t i)  const { return m_curves[i]; }
    [[nodiscard]] const Surface& surface(uint32_t i) const { return m_surfaces[i]; }

    [[nodiscard]] bool isClosed() const noexcept;

private:
    std::vector<Vertex>  m_verts;
    std::vector<Edge>    m_edges;
    std::vector<Coedge>  m_coedges;
    std::vector<Loop>    m_loops;
    std::vector<Face>    m_faces;
    std::vector<Shell>   m_shells;
    std::vector<Solid>   m_solids;
    std::vector<Curve>   m_curves;
    std::vector<Surface> m_surfaces;
    std::vector<NurbsSurface> m_nurbsSurfaces;  // exact surfaces referenced by Nurbs faces

    // Shared core of splitFace / imprintCurve: cut `faceId` between two of its
    // outer-loop vertices with a new edge. When `explicitCurve` is null a
    // straight Line chord is built (splitFace); otherwise the new edge carries
    // *explicitCurve with its param range set to reproduce the two endpoints
    // (imprintCurve). Returns the new face id, or kInvalid on failure.
    uint32_t cutFaceBetween(uint32_t faceId, uint32_t vA, uint32_t vB,
                            const Curve* explicitCurve);

    // Shared kill-edge-vertex core of joinEdges (requireSameCurve) and
    // mergeCollinearEdges (geometrically-collinear separate-curve Lines).
    bool joinEdgesImpl(uint32_t vertexId, bool requireSameCurve, Tolerance tol);
};

// ──────────── Primitives ─────────────────────────────────────────────────────

// An axis-aligned analytic box centred at the origin: 8 vertices, 12 line
// edges, 6 planar faces, one closed shell. The canonical analytic-B-rep proof.
[[nodiscard]] Body makeBox(float width, float height, float depth);

// A capped cylinder along +Z (n side quads on a Cylinder surface + two planar
// n-gon caps). Watertight solid, euler 2.
[[nodiscard]] Body makeCylinder(float radius, float height, uint32_t segments);

// A cone along +Z (apex + bottom ring: n triangular sides + one planar cap).
// Watertight solid, euler 2.
[[nodiscard]] Body makeCone(float radius, float height, uint32_t segments);

// A UV sphere (pole triangle fans + latitude quad bands) on a Sphere surface.
// Watertight solid, euler 2. latSegments ≥ 2, lonSegments ≥ 3.
[[nodiscard]] Body makeSphere(float radius, uint32_t latSegments, uint32_t lonSegments);

// ──────────── Creation ───────────────────────────────────────────────────────

// Extrude a closed planar profile (≥3 CCW points, seen from +its plane normal)
// along `dir` into a prism solid: a bottom cap, a top cap (profile + dir), and
// one quad side per profile edge, assembled watertight with outward-consistent
// winding. Generalises makeBox. Returns an empty Body for degenerate input:
// fewer than 3 points, a non-finite coordinate, a near-zero-area profile, or a
// `dir` parallel to the profile plane (zero projected height).
[[nodiscard]] Body extrudeProfile(const std::vector<Vec3>& profile, const Vec3& dir);

// Loft (ruled solid) between two closed planar profiles with the SAME vertex
// count and CORRESPONDING order (vertex i of `bottom` pairs with vertex i of
// `top`): a bottom cap, a top cap, and one quad side per matching edge pair,
// assembled watertight with outward-consistent winding. Generalises
// extrudeProfile (a loft whose top is the bottom translated). For similar
// profiles scaled about a common axis the sides are planar trapezoids and the
// volume is the exact frustum h/3·(A₀+A₁+√(A₀·A₁)). Returns an empty Body for
// degenerate input: fewer than 3 points, mismatched vertex counts, a non-finite
// coordinate, a near-zero-area profile, or a top whose centroid lies in the
// bottom's plane (no offset → an undefined loft). Inconsistent windings that
// cannot sew manifold also yield an empty Body.
[[nodiscard]] Body loftProfiles(const std::vector<Vec3>& bottom, const std::vector<Vec3>& top);

// Pyramid (or cone): a closed planar `base` polygon (≥3 points) fanned to a single
// `apex` point off the base plane — one triangular side per base edge plus the
// base cap, watertight with outward-consistent winding (the base is reversed when
// needed so the apex is on the base normal's +side → positive volume). An n-gon
// base gives a faceted cone. Genus 0 (euler 2); volume = ⅓·A·h where A is the base
// area and h the perpendicular apex height. Returns an empty Body for fewer than 3
// base points, a non-finite coordinate, a near-zero-area base, or an apex lying in
// the base plane (zero height).
[[nodiscard]] Body makePyramid(const std::vector<Vec3>& base, const Vec3& apex);

// An ALL-PLANAR faceted cylinder: a regular `segments`-gon prism along +Z of the
// given radius/height, centred at the origin. Unlike makeCylinder (whose side
// carries a curved Cylinder surface), every face here is a Plane, so it composes
// with the planar boolean — the pragmatic faceted path for curved-solid booleans.
// segments ≥ 3; watertight, euler 2.
[[nodiscard]] Body makeFacetedCylinder(float radius, float height, uint32_t segments);

// An ALL-PLANAR faceted TUBE (hollow cylinder / pipe / washer) along +Z, centred
// at the origin: `segments`-gon outer and inner walls plus annular top and bottom
// caps (4·segments planar quads), built DIRECTLY (not via a boolean, so it is
// robust at any segment count). The through-hole makes it genus 1 → euler 0
// (a single connected boundary), watertight; faceted material volume =
// ½·segments·(outerRadius²−innerRadius²)·sin(2π/segments)·height → π(R²−r²)h as
// segments grow. segments ≥ 3. Returns empty for outerRadius ≤ innerRadius,
// innerRadius ≤ 0, height ≤ 0, or a non-finite parameter.
[[nodiscard]] Body makeTube(float outerRadius, float innerRadius, float height,
                            uint32_t segments);

// An ALL-PLANAR faceted sphere (a UV-sphere-like polyhedron): a polygonal
// semicircle of `latSegments` bands revolved in `lonSegments` steps, all faces
// Plane, so it composes with the planar boolean. latSegments ≥ 2, lonSegments ≥
// 3; watertight, euler 2. The smooth-sphere stand-in for booleans.
[[nodiscard]] Body makeFacetedSphere(float radius, uint32_t latSegments, uint32_t lonSegments);

// Revolve a closed planar profile a full 360° about an axis into a solid of
// revolution (a ring/torus-like solid), in `segments` (≥3) angular steps: each
// profile edge sweeps a band of quad faces; the full revolution needs no end
// caps. The profile must lie to ONE side of the axis (not cross or touch it).
// Returns an empty Body for degenerate input: fewer than 3 profile points,
// segments < 3, a non-finite coordinate, a zero-length axis, a near-zero-area
// profile, or a profile that touches/crosses the axis. Watertight; being
// torus-like its boundary has euler characteristic 0 (genus 1).
[[nodiscard]] Body revolveProfile(const std::vector<Vec3>& profile, const Vec3& axisOrigin,
                                  const Vec3& axisDir, uint32_t segments);

// Partial revolve: sweep a closed planar profile through `sweepRadians` (0 < θ <
// 2π) into a watertight ARC solid — the swept side bands PLUS two planar END CAPS
// (the profile at angle 0 and at θ). Unlike the full revolve (a genus-1 ring),
// the capped arc is genus 0 (euler 2). A profile that TOUCHES the axis is
// supported: on-axis vertices weld to a single pole and the adjacent bands
// collapse to triangle fans, giving a pie-slice / cylindrical-sector solid (still
// genus 0, euler 2). For a non-axis-touching (ring-offset) profile the volume is
// (θ/2π)·(full Pappus volume) = θ·R̄·A. θ ≥ 2π−ε delegates to the full
// revolveProfile (byte-identical). Returns an empty Body for degenerate input:
// θ ≤ 0 or non-finite, fewer than 3 profile points, segments < 3, a non-finite
// coordinate, a zero-length axis, a near-zero-area profile, a profile that
// CROSSES to the other side of the axis, or one lying entirely on the axis.
[[nodiscard]] Body revolveProfilePartial(const std::vector<Vec3>& profile, const Vec3& axisOrigin,
                                         const Vec3& axisDir, uint32_t segments, float sweepRadians);

// ──────────── Boolean building blocks ────────────────────────────────────────

// Mutual imprint of two overlapping solids — the B-rep boolean's SEGMENTATION
// step. Imprints onto each body every intersection Line where a planar face of
// the OTHER body transects one of its faces (intersectSurfaces → imprintCurve),
// iterating to a fixpoint. Afterwards no face of either body straddles the
// other's boundary: every face is entirely Inside, entirely Outside, or on the
// other solid — the precondition for selecting and sewing sub-faces per boolean
// op. Both bodies are modified in place; checkIntegrity / checkGeometry stay
// clean. Handles planar (Line-imprint) faces; curved-face imprint is a
// follow-up. Deterministic.
void imprintMutually(Body& a, Body& b, Tolerance tol = {});

// EXACT segment-vs-triangle intersection test built on the Shewchuk adaptive
// orient3D predicate — an exact building block for the boolean's ray-parity /
// segment-intersection decisions (planar paths; a fully-exact ray parity over a
// tessellated CURVED shell additionally needs Simulation-of-Simplicity, since
// pole triangles are near-coplanar with interior query points).
// Returns true iff the segment A→B pierces the triangle (v0,v1,v2) interior: A
// and B strictly on opposite sides of the triangle's plane AND the line A→B
// passing on the same rotational side of all three edges — every branch decided
// from the EXACT sign of orient3D. A coplanar segment or a degenerate (zero-area)
// triangle contributes nothing (returns false, no flag). `degenerate` is set
// (and the result is false) when EXACTLY one endpoint lies on the plane or the
// line grazes an edge/vertex exactly (an exact orient3D zero), so the caller can
// re-cast. A near-miss is NOT flagged, unlike a float barycentric band, which
// both false-flags near-misses and can mis-sign true crossings under
// cancellation. Deterministic.
[[nodiscard]] bool segmentCrossesTriangleExact(const Vec3& A, const Vec3& B, const Vec3& v0,
                                               const Vec3& v1, const Vec3& v2, bool& degenerate);

}  // namespace nexus::geometry::brep
