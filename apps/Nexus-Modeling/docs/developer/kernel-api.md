# Nexus Modeling — Geometry Kernel Reference

*Complete API reference for the geometry kernel (nexus::geometry)*

---

## Module Overview

The geometry kernel (`nexus::geometry`) is a **standalone library** with zero dependencies on UI, rendering, or application logic. It can be used headless for batch processing, CLI tools, or server-side geometry processing.

```
nexus::geometry
├── Mesh.*              // Indexed n-gon mesh, attributes, topology
├── HalfEdgeMesh.*      // Half-edge structure, Euler ops, validators
├── MeshBoolean.*       // CSG: tri-tri → retriangulate → cut → classify → stitch
├── SubdivisionSurface.* // Catmull-Clark, Loop, crease weights
├── MeshRemesh.*        // Quad remesh, voxel remesh, decimation
├── MeshDecimator.*     // Quadric error decimation
├── MeshLaplacian.*     // Smoothing, fairing, parameterization
├── MeshBoolean.*       // CSG: tri-tri → retriangulate → cut → classify → stitch
├── MeshBVH.*           // Bounding volume hierarchy for ray queries
├── Delaunay2D.*        // Constrained Delaunay triangulation
├── ConstrainedDelaunay2D.*
├── TetDelaunay3D.*     // 3D Delaunay for tetrahedralization
├── AnalyticBRep.*      // Analytic B-rep (Box/Cylinder/Sphere/Cone/Torus)
├── Tolerance.h          // Central tolerance system
├── RobustPredicates.*   // Exact orient2D/3D, incircle/insphere
└── ... (60+ files)
```

---

## Quick Start

```cpp
#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/MeshBoolean.h>
#include <nexus/geometry/primitives.h>
#include <nexus/geometry/AnalyticBRep.h>

using namespace nexus::geometry;
using namespace nexus::render;

// Create primitives
Mesh box = primitives::makeBox(2.0f, 3.0f, 4.0f);
Mesh sphere = primitives::makeSphere(1.5f);
Mesh cylinder = primitives::makeCylinder(1.0f, 5.0f);

// Boolean operations
auto result = MeshBoolean::compute(box, sphere, BooleanOp::Difference);

// Analytic B-rep primitives
auto box = brep::makeBox(10, 20, 5);
auto cylinder = brep::makeCylinder(5, 10);
auto sphere = brep::makeSphere(5);

// Convert to mesh for rendering
Mesh mesh = brepBody.toMesh();
```

---

## Core Types

### `nexus::render::Vec3` / `Vec4`
```cpp
struct Vec3 { float x, y, z; };
// Operators: +, -, *, /, dot(), cross(), length(), normalize()
// Constructors: Vec3(), Vec3(float), Vec3(x,y,z)
```

### `nexus::geometry::Mesh`
```cpp
class Mesh {
    // Topology
    uint32_t vertexCount() const;
    uint32_t faceCount() const;
    const std::vector<Vec3>& positions() const;
    const std::vector<Face>& topology() const;
    
    // Attributes
    bool hasNormals() const;
    const std::vector<Vec3>& normals() const;
    bool hasUVs() const;
    const std::vector<Vec2>& uvs() const;
    
    // Mutation
    void setPositions(std::vector<Vec3>&&);
    void setNormals(std::vector<Vec3>&&);
    bool computeVertexNormals();  // Recompute from faces
    
    // Validation
    bool isValid() const;
    Aabb computeBounds() const;
    float volume() const;          // Signed volume (closed mesh)
    float surfaceArea() const;
};
```

### `nexus::geometry::Face`
```cpp
struct Face {
    std::vector<uint32_t> indices;  // Vertex indices (CCW winding)
    uint32_t material = 0;           // Material slot
};
```

### `nexus::render::Aabb`
```cpp
struct Aabb {
    Vec3 min, max;
    Vec3 center() const;
    Vec3 extents() const;
    bool contains(const Vec3& p) const;
    bool intersects(const Aabb& other) const;
    void expand(const Vec3& p);
    void expand(const Aabb& other);
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
    Mesh makeTorus(float majorR, float minorR, uint32_t majorSeg, uint32_t minorSeg);
}
```

### From Raw Data
```cpp
Mesh mesh;
mesh.attributes().setPositions(std::move(positions));
mesh.topology().addFace({0, 1, 2});  // Triangle
mesh.topology().addFace({0, 1, 2, 3});  // Quad (auto-triangulated)
mesh.topology().addFace(Face{indices, materialId});
mesh.attributes().setPositions(std::move(positions));
// Optional
mesh.attributes().setNormals(std::move(normals));
mesh.attributes().setUVs(std::move(uvs));
mesh.computeVertexNormals();  // Auto-compute from faces
```

---

## Mesh Operations

### Boolean / CSG
```cpp
enum class BooleanOp { Union, Difference, Intersection };

struct BooleanResult {
    bool ok;
    std::string error;
    Mesh result;
    std::vector<uint32_t> faceClassification;  // Inside/Outside/OnSurface
};

BooleanResult MeshBoolean::compute(const Mesh& A, const Mesh& B, BooleanOp op);
// Returns exact CSG result with exact volumes, watertight 2-manifold
```

### Remeshing & Decimation
```cpp
// Voxel remesh (uniform)
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

// Decimation
struct DecimationOptions {
    uint32_t targetFaceCount;
    bool preserveBoundary = true;
    float boundaryWeight = 10.0f;
};
std::pair<Mesh, std::vector<uint32_t>> MeshDecimator::decimate(const Mesh&, const DecimationOptions&);
// Returns {decimatedMesh, oldToNewVertexMap}
```

### Subdivision
```cpp
struct SubdivisionOptions {
    uint32_t levels = 1;
    enum class Scheme { CatmullClark, Loop } scheme = Scheme::CatmullClark;
    std::vector<uint32_t> creaseEdges;  // Edge indices to crease
    std::vector<float> creaseWeights;   // 0=smooth, 1=sharp
};
std::optional<Mesh> SubdivisionSurface::catmullClark(const Mesh&, const SubdivisionOptions&);
```

### Boolean Operations (Real CSG Pipeline)
```cpp
enum class BooleanOp { Union, Difference, Intersection };

struct BooleanResult {
    bool ok;
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
```

### Decimation
```cpp
struct DecimationOptions {
    uint32_t targetFaceCount;
    bool preserveBoundary = true;
    float boundaryWeight = 10.0f;
};

std::pair<Mesh, std::vector<uint32_t>> MeshDecimator::decimate(const Mesh&, const DecimationOptions&);
// Returns {decimatedMesh, oldToNewVertexMap}
```

### Subdivision
```cpp
enum class SubdivisionScheme { CatmullClark, Loop };

struct SubdivisionOptions {
    uint32_t levels = 1;
    Scheme scheme = Scheme::CatmullClark;
    std::vector<uint32_t> creaseEdges;
    std::vector<float> creaseWeights;  // 0=smooth, 1=sharp
};

std::optional<Mesh> SubdivisionSurface::catmullClark(const Mesh&, const SubdivisionOptions&);
std::optional<Mesh> SubdivisionSurface::loopSubdivision(const Mesh&, const SubdivisionOptions&);
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

Full topological structure with Euler operators:

```cpp
class HalfEdgeMesh {
    // Construction
    static std::optional<HalfEdgeMesh> fromMesh(const Mesh&);
    Mesh toMesh() const;

    // Topology
    struct Vertex { uint32_t edge = kInvalid; Vec3 pos; };
    struct Edge { uint32_t vert[2] = {kInvalid, kInvalid}; uint32_t face[2] = {kInvalid, kInvalid}; };
    struct HalfEdge { uint32_t vertex, edge, face, next, prev, twin; };
    struct Face { uint32_t halfEdge = kInvalid; };

    // Local Euler operators (all integrity-clean)
    uint32_t insertEdgeVertex(uint32_t edgeId, float t);
    uint32_t splitEdge(uint32_t edgeId);
    void collapseEdge(uint32_t edgeId);
    uint32_t splitFace(uint32_t faceId, uint32_t v0, uint32_t v1);
    void flipEdge(uint32_t edgeId);
    void collapseFace(uint32_t faceId);
    void pokeFace(uint32_t faceId);
    void insertEdgeLoop(uint32_t edgeId, float t);
    
    // Queries
    std::vector<uint32_t> vertexRing(uint32_t vert) const;
    std::vector<uint32_t> faceRing(uint32_t face) const;
    std::vector<uint32_t> boundaryLoops() const;
    
    // Validation
    struct IntegrityReport { bool ok; std::string reason; ... };
    IntegrityReport checkIntegrity() const;
    
    // To/from indexed mesh
    static std::optional<HalfEdgeMesh> fromMesh(const Mesh&);
    Mesh toMesh() const;
};
```

### Euler Operators (all integrity-clean)
```cpp
// Local ops (all 6 integrity-clean, property-tested)
insertEdgeVertex    // split edge, create vertex + 2 edges + 2 coedges
splitEdge           // same, but explicit position
collapseEdge        // merge vertices, remove edge
splitFace           // add diagonal edge across face
flipEdge            // flip interior edge (Delaunay)
collapseFace        // remove face, merge edges
pokeFace            // insert center vertex, fan triangulate
```

---

## Analytic B-rep (`nexus::geometry::brep`)

Exact boundary representation with analytic geometry (not tessellated).

```cpp
namespace nexus::geometry::brep {
    // Topology
    struct Vertex { Vec3 point; uint32_t coedge = kInvalid; };
    struct Edge { uint32_t curve, v0, v1; float t0=0, t1=1; uint32_t coedge = kInvalid; };
    struct Coedge { uint32_t edge; bool reversed; uint32_t loop, next, prev, partner; };
    struct Loop { uint32_t face; uint32_t first; bool outer; };
    struct Face { uint32_t surface; bool reversed; uint32_t outerLoop; std::vector<uint32_t> innerLoops; uint32_t shell; };
    struct Shell { std::vector<uint32_t> faces; bool closed = false; };
    struct Solid { std::vector<uint32_t> shells; };  // [0]=outer, rest=voids
    
    // Curves
    enum CurveKind { Line, Circle, Nurbs };
    struct Curve { CurveKind kind; Vec3 origin, dir, ref; float radius; uint32_t nurbs; };
    
    // Surfaces
    enum SurfaceKind { Plane, Cylinder, Sphere, Torus, Cone, Nurbs };
    struct Surface {
        SurfaceKind kind;
        Vec3 origin, normal, uAxis;
        float radius;
        uint32_t nurbs;
        Vec3 eval(float u, float v) const noexcept;
        Vec3 normalAt(float u, float v) const noexcept;
        Vec3 vAxis() const noexcept;
    };
    
    class Body {
        // ... topology storage ...
        std::optional<Body> fromFaces(points, faces);  // Build from face rings
        IntegrityReport checkIntegrity() const;        // Full validation
        Mesh toMesh() const;                           // Tessellate for rendering
        bool isClosed() const noexcept;
    };
    
    // Primitives
    Body makeBox(float w, float h, float d);
    Body makeCylinder(float r, float h, uint32_t seg);
    Body makeSphere(float r, uint32_t lat, uint32_t lon);
    Body makeCone(float r, float h, uint32_t seg);
    Body makeTorus(float R, float r, uint32_t major, uint32_t minor);
    // Boolean: union/diff/intersect (exact, analytic)
}
```

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
MeshExportReport MeshIO::exportMesh(const Mesh&, const std::string& path, const MeshExportOptions&);

// Import
MeshImportReport MeshIO::importMesh(const std::string& path, Mesh& out, const MeshImportOptions&);

// Supported formats:
 // Export: OBJ, STL (binary/ASCII), PLY, glTF 2.0 (GLB/GLTF)
 // Import:  OBJ, STL, PLY, glTF 2.0 (GLB/GLTF)
```

---

## Tolerance System

Centralized, scale-aware tolerance management:

```cpp
#include <nexus/geometry/Tolerance.h>

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

Exact geometric predicates using Shewchuk's adaptive expansion arithmetic:

```cpp
#include <nexus/geometry/RobustPredicates.h>

// 2D
int orient2d(const Vec2& a, const Vec2& b, const Vec2& c);  // +1: CCW, -1: CW, 0: collinear
int incircle(const Vec2& a, const Vec2& b, const Vec2& c, const Vec2& d);  // inside circle?

// 3D
int orient3d(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d);  // +1: CCW, -1: CW
int insphere(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d, const Vec3& e);

// Exact arithmetic: adaptive precision, no rounding errors
// Falls back to double → long double → adaptive expansion as needed
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

```cpp
// Export
MeshIO::exportMesh(mesh, "model.obj", MeshExportOptions{MeshExportFormat::OBJ});
MeshIO::exportMesh(mesh, "model.stl", MeshExportOptions{MeshExportFormat::STL});
MeshIO::exportMesh(mesh, "model.gltf", MeshExportOptions{MeshExportFormat::GLTF});

// Import
MeshImportReport rep = MeshIO::importMesh("model.obj", mesh, MeshImportOptions{});
if (!rep.valid) { /* handle errors */ }
```

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