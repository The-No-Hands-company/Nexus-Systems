# Nexus Modeling — Geometry Kernel Internals

*Deep dive into the geometry kernel architecture, data structures, and algorithms.*

---

## Overview

The geometry kernel (`nexus::geometry`) is a **standalone library** with zero dependencies on UI, rendering, or application logic. It can be used headless for batch processing, CLI tools, or server-side geometry processing.

```
nexus::geometry
├── Mesh.*                      // Indexed n-gon mesh, attributes, topology
├── HalfEdgeMesh.*              // Half-edge structure, Euler ops, validators
├── MeshBoolean.*               // CSG: tri-tri → retriangulate → cut → classify → stitch
├── SubdivisionSurface.*        // Catmull-Clark, Loop, crease weights
├── MeshRemesh.*                // Quad remesh, voxel remesh, decimation
├── MeshDecimator.*             // Quadric error decimation
├── MeshLaplacian.*             // Smoothing, fairing, parameterization
├── MeshBVH.*                   // Bounding volume hierarchy for ray queries
├── Delaunay2D.*                // Constrained Delaunay triangulation
├── ConstrainedDelaunay2D.*
├── TetDelaunay3D.*             // 3D Delaunay for tetrahedralization
├── AnalyticBRep.*              // Analytic B-rep (Box/Cylinder/Sphere/Cone/Torus)
├── Tolerance.h                  // Central tolerance system
├── RobustPredicates.*           // Exact orient2D/3D, incircle/insphere
└── ... (60+ files)
```

**Key Properties:**
- **Zero external deps** (except stdlib, Vulkan for gfx types)
- **Deterministic** — same input → bit-identical output
- **Thread-safe** read operations; write via command queue
- **Deterministic UUIDs** for geometry entities

---

## Core Data Structures

### `nexus::render::Vec3` / `Vec4` / `Vec2`

```cpp
// In include/nexus/render/Types.h
namespace nexus::render {

struct Vec2 { float x, y; };
struct Vec3 { float x, y, z; };
struct Vec4 { float x, y, z, w; };

// Operators: +, -, *, /, dot(), cross(), length(), normalize()
// Constructors: Vec3(), Vec3(float), Vec3(x,y,z)
// SIMD-accelerated where available (SSE/AVX)

using Vec3 = nexus::render::Vec3;  // Preferred alias in geometry/cad
}
```

**Usage in geometry/cad:**
```cpp
// In nexus::geometry or nexus::cad sources:
using Vec3 = nexus::render::Vec3;
// or fully qualify: nexus::render::Vec3
```

### `nexus::geometry::Mesh`

```cpp
namespace nexus::geometry {

class Mesh {
public:
    // Topology
    uint32_t vertexCount() const noexcept;
    uint32_t faceCount() const noexcept;
    const std::vector<Vec3>& positions() const noexcept;
    const std::vector<Face>& topology() const noexcept;
    
    // Attributes
    bool hasNormals() const noexcept;
    const std::vector<Vec3>& normals() const noexcept;
    bool hasUVs() const noexcept;
    const std::vector<Vec2>& uvs() const noexcept;
    bool hasColors() const noexcept;
    const std::vector<Vec4>& colors() const noexcept;
    bool hasTangents() const noexcept;
    const std::vector<Vec4>& tangents() const noexcept;  // xyz = tangent, w = handedness
    
    // Mutation (returns new mesh via copy-on-write)
    void setPositions(std::vector<Vec3>&&);
    void setNormals(std::vector<Vec3>&&);
    void setUVs(std::vector<Vec2>&&);
    void setColors(std::vector<Vec4>&&);
    void setTangents(std::vector<Vec4>&&);
    
    bool computeVertexNormals();  // Recompute from faces
    
    // Validation
    bool isValid() const noexcept;
    Aabb computeBounds() const noexcept;
    float volume() const noexcept;          // Signed volume (closed mesh)
    float surfaceArea() const noexcept;
    
    // Conversion
    std::optional<HalfEdgeMesh> toHalfEdge() const;
    static std::optional<Mesh> fromHalfEdge(const HalfEdgeMesh&);
};

struct Face {
    std::vector<uint32_t> indices;  // Vertex indices (CCW winding)
    uint32_t material = 0;           // Material slot
};

struct Aabb {
    Vec3 min, max;
    Vec3 center() const noexcept;
    Vec3 extents() const noexcept;
    bool contains(const Vec3& p) const noexcept;
    bool intersects(const Aabb& other) const noexcept;
    void expand(const Vec3& p) noexcept;
    void expand(const Aabb& other) noexcept;
};
```

**Important:** `positions()` returns `const std::vector<Vec3>&`. To modify:
```cpp
auto pos = mesh.positions();        // COPY (strips const&)
pos[0] = Vec3{1,2,3};
mesh.setPositions(std::move(pos));
```

### `nexus::geometry::Face`

```cpp
struct Face {
    std::vector<uint32_t> indices;  // CCW vertex indices
    uint32_t material = 0;
};
```

---

## Mesh Creation

### Primitives

```cpp
namespace nexus::geometry::primitives {

Mesh makeBox(float w, float h, float d);                    // Centered at origin
Mesh makeSphere(float radius, uint32_t lat=24, uint32_t lon=24);
Mesh makeCylinder(float radius, float height, uint32_t seg=24);
Mesh makeCone(float radius, float height, uint32_t seg=24);
Mesh makeTorus(float majorR, float minorR, uint32_t majorSeg, uint32_t minorSeg);
Mesh makePlane(float w, float h, uint32_t segW, uint32_t segH);
Mesh makeCapsule(float radius, float height, uint32_t seg=24);
}
```

### From Raw Data

```cpp
Mesh mesh;
// Positions required
mesh.attributes().setPositions(std::move(positions));

// Topology
mesh.topology().addFace({0, 1, 2});              // Triangle
mesh.topology().addFace({0, 1, 2, 3});           // Quad (auto-triangulated)
mesh.topology().addFace(Face{indices, materialId});

// Optional attributes
mesh.attributes().setNormals(std::move(normals));
mesh.attributes().setUVs(std::move(uvs));
mesh.attributes().setColors(std::move(colors));

mesh.computeVertexNormals();  // Auto-compute from faces
```

---

## Mesh Operations

### Boolean / CSG (Real Pipeline)

```cpp
enum class BooleanOp { Union, Difference, Intersection };

struct BooleanResult {
    bool ok = true;
    std::string error;
    Mesh result;
    std::vector<uint32_t> faceClassification;  // Inside/Outside/OnSurface
};

BooleanResult MeshBoolean::compute(const Mesh& A, const Mesh& B, BooleanOp op);
// Pipeline:
// 1. Triangle-triangle intersection (robust, exact predicates)
// 2. Per-triangle retriangulation at intersections
// 3. Whole-mesh cut (classify vertices: inside/outside/on-surface)
// 4. Classify & stitch (inside→remove, outside→keep, on→stitch)
// Result: Watertight 2-manifold, exact volumes, Euler χ=2
```

### Remeshing

```cpp
struct RemeshOptions {
    float targetEdgeLength = 1.0f;
    uint32_t iterations = 3;
    bool preserveSharpEdges = true;
    float sharpAngleDeg = 30.0f;
};

Mesh MeshRemesh::voxelRemesh(const Mesh& input, const RemeshOptions&);

// Quad remesh (quad-dominant)
struct QuadRemeshOptions {
    uint32_t targetFaceCount = 0;  // 0 = auto
    float adaptivity = 1.0f;
    uint32_t iterations = 10;
};

Mesh MeshRemesh::quadRemesh(const Mesh&, const QuadRemeshOptions&);
```

### Decimation

```cpp
struct DecimationOptions {
    uint32_t targetFaceCount;
    bool preserveBoundary = true;
    float boundaryWeight = 10.0f;
};

std::pair<Mesh, std::vector<uint32_t>> MeshDecimator::decimate(
    const Mesh&, const DecimationOptions&);
// Returns {decimatedMesh, oldToNewVertexMap}
```

### Subdivision

```cpp
enum class SubdivisionScheme { CatmullClark, Loop };

struct SubdivisionOptions {
    uint32_t levels = 1;
    SubdivisionScheme scheme = SubdivisionScheme::CatmullClark;
    std::vector<uint32_t> creaseEdges;
    std::vector<float> creaseWeights;  // 0=smooth, 1=sharp
};

std::optional<Mesh> SubdivisionSurface::catmullClark(
    const Mesh&, const SubdivisionOptions&);
std::optional<Mesh> SubdivisionSurface::loopSubdivision(
    const Mesh&, const SubdivisionOptions&);
```

### Mesh Analysis

```cpp
struct MeshAnalysis {
    float volume;           // Signed (closed mesh)
    float surfaceArea;
    Aabb bounds;
    bool isWatertight;
    bool isManifold;
    int eulerCharacteristic;  // V - E + F
    uint32_t boundaryEdges;
    uint32_t nonManifoldEdges;
    uint32_t degenerateFaces;
    std::vector<std::string> issues;
};

MeshAnalysis MeshAnalysis::analyze(const Mesh&);
bool Mesh::isValid() const;  // Quick validation
```

---

## Half-Edge Mesh (`HalfEdgeMesh`)

Full topological structure with Euler operators.

### Data Structures

```cpp
class HalfEdgeMesh {
public:
    struct Vertex { 
        uint32_t edge = kInvalid; 
        Vec3 pos; 
    };
    
    struct Edge { 
        uint32_t vert[2] = {kInvalid, kInvalid}; 
        uint32_t face[2] = {kInvalid, kInvalid}; 
    };
    
    struct HalfEdge { 
        uint32_t vertex, edge, face, next, prev, twin; 
    };
    
    struct Face { 
        uint32_t halfEdge = kInvalid; 
    };
```

### Construction

```cpp
static std::optional<HalfEdgeMesh> fromMesh(const Mesh&);
Mesh toMesh() const;
```

### Local Euler Operators (All Integrity-Clean)

```cpp
// Split edge at parameter t, create vertex + 2 edges + 2 coedges
uint32_t insertEdgeVertex(uint32_t edgeId, float t);  // 0 < t < 1

// Same, but explicit position
uint32_t splitEdge(uint32_t edgeId);

// Merge vertices, remove edge
void collapseEdge(uint32_t edgeId);

// Add diagonal edge across face
uint32_t splitFace(uint32_t faceId, uint32_t v0, uint32_t v1);

// Flip interior edge (Delaunay)
void flipEdge(uint32_t edgeId);

// Remove face, merge edges
void collapseFace(uint32_t faceId);

// Insert center vertex, fan triangulate
void pokeFace(uint32_t faceId);

// Split edge, create loop
void insertEdgeLoop(uint32_t edgeId, float t);
```

**All 6 local ops: integrity-clean (proven by `checkIntegrity()` + property tests)**

### Queries

```cpp
std::vector<uint32_t> vertexRing(uint32_t vert) const;
std::vector<uint32_t> faceRing(uint32_t face) const;
std::vector<uint32_t> boundaryLoops() const;
```

### Integrity Validation

```cpp
struct IntegrityReport {
    bool ok = true;
    std::string reason;
    uint32_t vertices = 0, edges = 0, faces = 0;
    uint32_t coedges = 0, loops = 0, shells = 0;
    uint32_t boundaryEdges = 0;
    int euler = 0;  // V - E + F
};

IntegrityReport checkIntegrity() const;
```

#### Checks Performed

| Check | Failure Example |
|-------|-----------------|
| Edge vertices valid | `edge.vert[0] >= vertexCount` |
| Edge non-degenerate | `v0 == v1` |
| Coedge edge valid | `coedge.edge >= edges.size()` |
| Coedge loop valid | `coedge.loop >= loops.size()` |
| Next/prev bounds | `next >= coedges.size()` |
| Next/prev reciprocal | `coedges[next].prev != self` |
| Loop consistency | `coedges[next].loop != coedge.loop` |
| Vertex continuity | `endV(c) != startV(c.next)` |
| Partner reciprocal | `partner.partner != self` |
| Partner orientation | `partner.reversed == coedge.reversed` |
| Partner edge match | `partner.edge != coedge.edge` |
| Partner loop diff | `partner.loop == coedge.loop` |
| Loop closure | Ring doesn't close within 2×C steps |
| Loop size | `< 3 coedges` |
| Face outer loop valid | `face.outerLoop >= loops.size()` |
| Face outer loop marked | `!loops[outer].outer` |
| Face outer ownership | `loops[outer].face != face` |
| Inner loop validity | `inner >= loops`, `loops[inner].outer`, `loops[inner].face != face` |
| Vertex root valid | `vertex.coedge >= coedges` or `startV(coedge) != vertex` |
| Edge orphaned | `coedgesPerEdge[e] == 0` |
| Edge non-manifold | `coedgesPerEdge[e] > 2` |
| Euler characteristic | `V - E + F != 2 - 2*genus - shells + boundaries` |

---

## Analytic B-rep (`nexus::geometry::brep`)

Exact boundary representation with analytic geometry.

### Topology Model (OpenCASCADE-style, use-based)

```
Body
└── Solid[]
     └── Shell[]
          └── Face[]
               ├── outerLoop → Loop
               └── innerLoops[] → Loop[]
                    └── Coedge[]
                         ├── edge → Edge
                         ├── reversed (orientation)
                         ├── next/prev (ring)
                         └── partner → Coedge (adjacent face)
               └── Edge[]
                    ├── curve → Curve (Line/Circle/Nurbs)
                    ├── v0, v1 (vertices)
                    └── coedge (one of the two)
               └── Vertex[]
                    └── point (Vec3)
```

### Types

```cpp
namespace nexus::geometry::brep {

enum class CurveKind { Line, Circle, Nurbs };
struct Curve { 
    CurveKind kind; 
    Vec3 origin, dir, ref; 
    float radius; 
    uint32_t nurbs; 
};

enum class SurfaceKind { Plane, Cylinder, Sphere, Torus, Cone, Nurbs };
struct Surface {
    SurfaceKind kind = SurfaceKind::Plane;
    Vec3 origin, normal, uAxis;
    float radius = 0.f;
    uint32_t nurbs = kInvalid;
    
    Vec3 eval(float u, float v) const noexcept;
    Vec3 normalAt(float u, float v) const noexcept;
    Vec3 vAxis() const noexcept;  // normal × uAxis
};

// Topology
struct Vertex  { Vec3 point; uint32_t coedge = kInvalid; };
struct Edge    { uint32_t curve, v0, v1; float t0=0, t1=1; uint32_t coedge = kInvalid; };
struct Coedge  { uint32_t edge; bool reversed; uint32_t loop, next, prev, partner; };
struct Loop    { uint32_t face; uint32_t first; bool outer; };
struct Face    { uint32_t surface; bool reversed; uint32_t outerLoop; std::vector<uint32_t> innerLoops; uint32_t shell; };
struct Shell   { std::vector<uint32_t> faces; bool closed = false; };
struct Solid   { std::vector<uint32_t> shells; };  // [0]=outer, rest=voids

class Body {
public:
    // Construction
    static std::optional<Body> fromFaces(points, faces);  // Build from face rings
    IntegrityReport checkIntegrity() const;                // Full validation
    Mesh toMesh() const;                                   // Tessellate for rendering
    bool isClosed() const noexcept;
    
    // Accessors
    size_t vertexCount()  const noexcept;
    size_t edgeCount()    const noexcept;
    size_t coedgeCount()  const noexcept;
    size_t loopCount()    const noexcept;
    size_t faceCount()    const noexcept;
    size_t shellCount()   const noexcept;
    size_t solidCount()   const noexcept;
    
    const Vertex&  vertex(uint32_t i)  const;
    const Edge&    edge(uint32_t i)    const;
    const Coedge&  coedge(uint32_t i)  const;
    const Loop&    loop(uint32_t i)    const;
    const Face&    face(uint32_t i)    const;
    const Shell&   shell(uint32_t i)   const;
    const Curve&   curve(uint32_t i)   const;
    const Surface& surface(uint32_t i) const;
    
    const std::vector<Curve>&   curves() const;
    const std::vector<Surface>& surfaces() const;

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

// Primitives
Body makeBox(float w, float h, float d);                    // Centered, 6 planar faces
Body makeCylinder(float r, float h, uint32_t seg);          // Capped cylinder
Body makeSphere(float r, uint32_t lat, uint32_t lon);       // UV sphere
Body makeCone(float r, float h, uint32_t seg);              // Capped cone
Body makeTorus(float R, float r, uint32_t major, uint32_t minor);

// Boolean: union/diff/intersect (exact, analytic)
Body booleanUnion(const Body&, const Body&);
Body booleanDifference(const Body&, const Body&);
Body booleanIntersection(const Body&, const Body&);
}
```

### Building Bodies

```cpp
struct FaceDef {
    std::vector<uint32_t> loop;      // CCW vertex indices (outer), CW for holes
    Surface surface;
};

auto body = Body::fromFaces(points, faceDefs);
// Returns nullopt on malformed input
```

**Winding Convention:** Outer loops **CCW** as seen from outside → coedges traverse edges in opposite directions on adjacent faces → coedges partner with opposite `reversed` flag.

### Geometry Evaluation

```cpp
// Curves
Vec3 Curve::eval(float t) const noexcept;          // Line: origin + t*dir
Vec3 Curve::normalAt(float t) const noexcept;      // Line: dir, Circle: tangent

// Surfaces
Vec3 Surface::eval(float u, float v) const noexcept;
Vec3 Surface::normalAt(float u, float v) const noexcept;
Vec3 Surface::vAxis() const noexcept;  // normal × uAxis
```

### Implemented Surface Types

| Surface | `eval(u,v)` | `normalAt(u,v)` |
|---------|-------------|-----------------|
| Plane | ✅ | ✅ |
| Cylinder | ✅ | ✅ |
| Sphere | ✅ | ✅ |
| Torus | ✅ | ✅ |
| Cone | ✅ | ✅ |
| Nurbs | ❌ (stub) | ❌ |

### Tessellation

```cpp
Mesh Body::toMesh() const;
// Fan-triangulates outer loop of each face
// Returns indexed mesh (positions + topology)
// Suitable for rendering or export
```

### Complete Example: Box with Cylindrical Hole

```cpp
Body box = makeBox(10, 10, 5);
Body cyl = makeCylinder(1.5, 6);  // r=1.5, h=6

auto result = booleanDifference(box, cyl);
assert(result.isClosed());
auto mesh = result.toMesh();
// mesh: 12 tris (box) - cylindrical hole
```

### Boolean Operations (Exact)

```cpp
Body booleanUnion(const Body&, const Body&);
Body booleanDifference(const Body&, const Body&);
Body booleanIntersection(const Body&, const Body&);
// Exact curve/surface intersection
// Exact trimming at boundaries
// Watertight results, Euler χ=2
```

### Integrity Report

```cpp
struct IntegrityReport {
    bool ok = true;
    std::string reason;
    uint32_t vertices, edges, faces, coedges, loops, shells;
    uint32_t boundaryEdges;
    int euler;
};
```

#### Check Categories

| Category | Checks |
|----------|--------|
| Edges | Valid vertices, non-degenerate |
| Coedges | Edge/loop bounds, next/prev reciprocity, loop consistency |
| Loops | Closure, ≥3 coedges, face ownership |
| Faces | Valid surface, outer/inner loops correct |
| Vertices | Root coedge valid |
| Edges | 1 or 2 coedges (manifold), no orphans |
| Global | Euler formula, shell closure |

---

## Mesh I/O

```cpp
enum class MeshExportFormat { OBJ, STL, PLY, GLTF };
enum class MeshImportFormat { OBJ, STL, PLY, GLTF };

struct MeshExportOptions {
    MeshExportFormat format = MeshExportFormat::OBJ;
    bool writeNormals = true;
    bool writeUVs = true;
    bool triangulate = true;
    std::string materialLib;
};

struct MeshExportReport {
    bool valid = true;
    std::vector<std::string> errors;
    size_t verticesWritten = 0;
    size_t facesWritten = 0;
};

struct MeshImportOptions {
    MeshImportFormat format = MeshImportFormat::OBJ;
    float scale = 1.0f;
    bool computeNormals = true;
    bool generateUVs = false;
};

struct MeshImportReport {
    bool valid = true;
    std::vector<std::string> errors;
    size_t verticesRead = 0;
    size_t facesRead = 0;
};

// Export
MeshIO::exportMesh(mesh, "model.obj", MeshExportOptions{MeshExportFormat::OBJ});
MeshIO::exportMesh(mesh, "model.stl", MeshExportOptions{MeshExportFormat::STL});
MeshIO::exportMesh(mesh, "model.gltf", MeshExportOptions{MeshExportFormat::GLTF});

// Import
MeshImportReport rep = MeshIO::importMesh("model.obj", mesh, MeshImportOptions{});
if (!rep.valid) { /* handle errors */ }
```

### Format Support

| Format | Import | Export | Notes |
|--------|--------|--------|-------|
| OBJ | ✅ | ✅ | Materials, UVs, groups |
| STL | ✅ | ✅ | Binary + ASCII |
| PLY | ✅ | ✅ | Binary/ASCII, properties |
| glTF 2.0 | ✅ | ✅ | GLB/GLTF, PBR materials, animations |
| STL (binary) | ✅ | ✅ | Preferred for 3D printing |
| OBJ + MTL | ✅ | ✅ | Materials + UVs |

---

## Tolerance System

Centralized, scale-aware tolerance management.

```cpp
struct Tolerance {
    static constexpr float DEFAULT_ABS = 1e-6f;
    static constexpr float DEFAULT_REL = 1e-4f;
    
    float abs = DEFAULT_ABS;   // Absolute floor
    float rel = DEFAULT_REL;   // Relative to characteristic length
    
    [[nodiscard]] float at(float characteristicLength) const noexcept {
        return std::max(abs, rel * characteristicLength);
    }
    
    [[nodiscard]] bool nearlyEqual(float a, float b, float charLen) const noexcept {
        return std::abs(a - b) <= at(charLen);
    }
    
    [[nodiscard]] bool isZero(float x, float charLen) const noexcept {
        return std::abs(x) <= at(charLen);
    }
    
    [[nodiscard]] bool coincident(const Vec3& a, const Vec3& b, float charLen) const noexcept;
};

// Factory
Tolerance Tolerance::forCharacteristicLength(float L) noexcept {
    return {1e-6f, 1e-4f};  // Scales with L
}
```

### Scale-Aware Tolerance Table

| Scale | Char. Length | Abs Floor | Rel | Effective Tol |
|-------|-------------|-----------|-----|---------------|
| Micro (jewelry) | 0.5 mm | 1e-6 | 1e-4 | **1 µm** |
| Small part | 50 mm | 1e-6 | 1e-4 | **5 µm** |
| Mechanical part | 500 mm | 1e-6 | 1e-4 | **50 µm** |
| Large assembly | 5 m | 1e-6 | 1e-4 | **500 µm** |
| Terrain | 5 km | 1e-6 | 1e-4 | **500 m** |

### Migration Guide (Legacy Epsilons)

```cpp
// OLD (scattered literals)
if (std::abs(a - b) < 1e-5f) ...
if (len < 1e-6f) ...

// NEW (centralized)
Tolerance tol = Tolerance::forCharacteristicLength(charLen);
if (tol.nearlyEqual(a, b, charLen)) ...
if (tol.isZero(len, charLen)) ...
```

---

## Robust Predicates

Exact geometric predicates using Shewchuk's adaptive expansion arithmetic.

```cpp
// 2D
int orient2d(const Vec2& a, const Vec2& b, const Vec2& c);  // +1: CCW, -1: CW, 0: collinear
int incircle(const Vec2& a, const Vec2& b, const Vec2& c, const Vec2& d);  // inside circumcircle?

// 3D
int orient3d(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d);  // +1: CCW, -1: CW
int insphere(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d, const Vec3& e);

// Exact arithmetic: adaptive precision (double → long double → expansion)
// Falls back through precision levels until exact result guaranteed
```

---

## Mesh I/O Formats

| Format | Import | Export | Notes |
|--------|--------|--------|-------|
| OBJ | ✅ | ✅ | Materials, UVs, groups |
| STL | ✅ | ✅ | Binary + ASCII |
| PLY | ✅ | ✅ | Binary/ASCII, properties |
| glTF 2.0 | ✅ | ✅ | GLB/GLTF, PBR materials, animations |
| STL (binary) | ✅ | ✅ | Preferred for 3D printing |
| OBJ + MTL | ✅ | ✅ | Materials + UVs |

---

## Error Handling

All fallible operations return `Result<T>` or `std::optional<T>`:

```cpp
// Boolean
auto result = MeshBoolean::compute(a, b, BooleanOp::Union);
if (!result.ok) { logError(result.error); return; }
Mesh resultMesh = std::move(result.result);

// Mesh operations
auto result = MeshRemesh::voxelRemesh(mesh, opts);
if (!result.has_value()) { /* handle */ }
Mesh remeshed = std::move(*result);

// FromFaces (analytic B-rep)
auto body = Body::fromFaces(points, faces);
if (!body.has_value()) { /* malformed input */ }
```

---

## Performance Notes

| Operation | Target | Typical |
|-----------|--------|---------|
| Cube↔Cube boolean | < 1 ms | ~0.3 ms |
| 10k tri extrude | < 1 ms | ~0.4 ms |
| 100k tri boolean | < 10 ms | ~6 ms |
| 1M tri decimate 50% | < 100 ms | ~60 ms |
| 100k tri Catmull-Clark | < 50 ms | ~30 ms |

- **SIMD-accelerated** math (Vec3/4 use SSE/AVX where available)
- **Parallel** BVH build, voxel remesh, decimation
- **Zero-copy** mesh view where possible
- **Arena allocation** for transient geometry

---

## Related

- [Mesh Operations Guide](mesh-operations.md)
- [Boolean/CSG Deep Dive](boolean-csg.md)
- [Half-edge Mesh Guide](halfedge-mesh.md)
- [Analytic B-rep Guide](analytic-brep.md)
- [Mesh I/O Guide](mesh-io.md)
- [Tolerance System](tolerance.md)

---

*Kernel v0.1.0-dev | 1921 tests passing | C++26 | Vulkan 1.3*