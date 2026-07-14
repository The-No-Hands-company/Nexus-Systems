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
#include <nexus/geometry/Tolerance.h>
#include <nexus/render/Camera.h>

#include <cstdint>
#include <optional>
#include <string>
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
};

struct Edge {
    uint32_t curve = kInvalid;
    uint32_t v0 = kInvalid, v1 = kInvalid;   // curve runs v0 → v1 over [t0,t1]
    float    t0 = 0.f, t1 = 1.f;
    uint32_t coedge = kInvalid;     // one coedge using this edge
};

struct Coedge {
    uint32_t edge = kInvalid;
    bool     reversed = false;      // true ⇒ traverses the edge v1 → v0
    uint32_t loop = kInvalid;
    uint32_t next = kInvalid, prev = kInvalid;  // around the loop
    uint32_t partner = kInvalid;    // the coedge on the adjacent face (same edge)
};

struct Loop {
    uint32_t face = kInvalid;
    uint32_t first = kInvalid;      // a coedge in the ring
    bool     outer = true;          // outer boundary vs inner hole
};

struct Face {
    uint32_t surface = kInvalid;
    bool     reversed = false;      // surface normal flipped vs face outward normal
    uint32_t outerLoop = kInvalid;
    std::vector<uint32_t> innerLoops;
    uint32_t shell = kInvalid;
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

    // One face definition for fromFaces(): a CCW vertex-index ring + its surface.
    struct FaceDef {
        std::vector<uint32_t> loop;
        Surface surface;
    };

    // Assemble a body from a point set and per-face vertex rings. Edges are
    // deduplicated across faces and coedges partnered (the analytic analog of
    // HalfEdgeMesh::fromMesh). Returns nullopt on malformed input.
    [[nodiscard]] static std::optional<Body> fromFaces(
        const std::vector<Vec3>& points, const std::vector<FaceDef>& faces);

    // Direct half-edge/analytic-invariant validation over all entities.
    [[nodiscard]] IntegrityReport checkIntegrity() const;

    // Polygonal tessellation of the shell (each face's outer loop, fan-
    // triangulated) — for display and cross-checking against the mesh validator.
    [[nodiscard]] Mesh toMesh() const;

    [[nodiscard]] size_t vertexCount() const noexcept { return m_verts.size(); }
    [[nodiscard]] size_t edgeCount()   const noexcept { return m_edges.size(); }
    [[nodiscard]] size_t coedgeCount() const noexcept { return m_coedges.size(); }
    [[nodiscard]] size_t loopCount()   const noexcept { return m_loops.size(); }
    [[nodiscard]] size_t faceCount()   const noexcept { return m_faces.size(); }
    [[nodiscard]] size_t shellCount()  const noexcept { return m_shells.size(); }
    [[nodiscard]] size_t solidCount()  const noexcept { return m_solids.size(); }

    [[nodiscard]] const Vertex&  vertex(uint32_t i) const { return m_verts[i]; }
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
};

// ──────────── Primitives ─────────────────────────────────────────────────────

// An axis-aligned analytic box centred at the origin: 8 vertices, 12 line
// edges, 6 planar faces, one closed shell. The canonical analytic-B-rep proof.
[[nodiscard]] Body makeBox(float width, float height, float depth);

}  // namespace nexus::geometry::brep
