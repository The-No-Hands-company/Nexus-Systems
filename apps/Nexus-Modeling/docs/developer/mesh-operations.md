# Nexus Modeling — Mesh Operations Guide

*Comprehensive guide to mesh operations in the geometry kernel.*

---

## Overview

The geometry kernel provides a complete suite of mesh operations through a clean, consistent API. All operations follow the same pattern: input validation → core algorithm → result construction → validation.

---

## Operation Categories

### 1. **Primitive Creation**
| Operation | Function | Description |
|-----------|----------|-------------|
| Box | `makeBox(w, h, d)` | Axis-aligned box |
| Sphere | `makeSphere(r, lat, lon)` | UV sphere |
| Cylinder | `makeCylinder(r, h, seg)` | Cylinder with end caps |
| Cone | `makeCone(r, h, seg)` | Cone with apex |
| Torus | `makeTorus(R, r, major, minor)` | Torus |
| Plane | `makePlane(w, h, segW, segH)` | Subdivided plane |

### 2. **Boolean / CSG**
```cpp
enum class BooleanOp { Union, Difference, Intersection };

struct BooleanResult {
    bool ok;
    std::string error;
    Mesh result;
    std::vector<uint32_t> faceClassification;  // Inside/Outside/OnSurface
};

BooleanResult MeshBoolean::compute(const Mesh& A, const Mesh& B, BooleanOp op);
```

### 3. **Remeshing**
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

### Remeshing Options Summary
| Operation | Use Case | Key Parameters |
|-----------|----------|----------------|
| Voxel Remesh | Uniform resolution, watertight | `targetEdgeLength`, `preserveSharpEdges` |
| Quad Remesh | Quad-dominant, animation-ready | `targetFaceCount`, `adaptivity` |
| Decimation | LOD, optimization | `targetFaceCount`, `boundaryWeight` |
| Catmull-Clark | Smooth subdivision | `levels`, `creaseEdges`, `creaseWeights` |
| Loop | Triangle subdivision | `levels`, `creaseEdges`, `creaseWeights` |

---

## Mesh Analysis & Validation

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

### Common Validation Checks
```cpp
if (!mesh.isValid()) { /* handle invalid */ }
auto analysis = MeshAnalysis::analyze(mesh);
if (!analysis.isWatertight) { /* handle non-watertight */ }
if (!analysis.isManifold) { /* handle non-manifold */ }
if (analysis.degenerateFaces > 0) { /* clean degenerate faces */ }
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
MeshIO::exportMesh(mesh, "model.obj", MeshExportOptions{MeshExportFormat::OBJ});
MeshIO::exportMesh(mesh, "model.stl", MeshExportOptions{MeshExportFormat::STL});
MeshIO::exportMesh(mesh, "model.gltf", MeshExportOptions{MeshExportFormat::GLTF});

// Import
MeshImportReport rep = MeshIO::importMesh("model.obj", mesh, MeshImportOptions{});
if (!rep.valid) { /* handle errors */ }
```

---

## Supported Formats

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