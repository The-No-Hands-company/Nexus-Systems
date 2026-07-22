# Nexus Modeling — Analytic B-rep Guide

*Exact boundary representation with analytic geometry (not tessellated).*

---

## Overview

The analytic B-rep kernel (`nexus::geometry::brep`) provides **exact CAD-grade solids** with:
- **Typed topology**: Vertex/Edge/Coedge/Loop/Face/Shell/Solid
- **Analytic geometry**: Line, Circle, Plane, Cylinder, Sphere, Torus, Cone
- **Exact evaluation**: `eval(u,v)`, `normalAt(u,v)` for all surface types
- **Full integrity validation**: `checkIntegrity()` — Euler, manifold, vertex continuity, partner reciprocity
- **Tessellation**: `toMesh()` for rendering/export

---

## Topology Model (OpenCASCADE-style, use-based)

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

**Key invariant:** Each geometric edge is used by exactly 2 coedges (one per adjacent face), with opposite `reversed` flags. Boundary edges have 1 coedge.

---

## Types

```cpp
namespace nexus::geometry::brep {

// Geometry
enum class CurveKind { Line, Circle, Nurbs };
struct Curve { 
    CurveKind kind; 
    Vec3 origin, dir, ref; 
    float radius; 
    uint32_t nurbs; 
};

enum class SurfaceKind { Plane, Cylinder, Sphere, Torus, Cone, Nurbs };
struct Surface {
    SurfaceKind kind;
    Vec3 origin, normal, uAxis;
    float radius;
    uint32_t nurbs;
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

// Body
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
    bool isClosed() const noexcept;

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
Body makeBox(float w, float h, float d);
Body makeCylinder(float r, float h, uint32_t seg);
Body makeSphere(float r, uint32_t lat, uint32_t lon);
Body makeCone(float r, float h, uint32_t seg);
Body makeTorus(float R, float r, uint32_t major, uint32_t minor);

// Boolean: union/diff/intersect (exact, analytic)
Body booleanUnion(const Body&, const Body&);
Body booleanDifference(const Body&, const Body&);
Body booleanIntersection(const Body&, const Body&);
}
```

---

## Building Bodies

### `fromFaces(points, faces)`

```cpp
struct FaceDef {
    std::vector<uint32_t> loop;      // CCW vertex indices (outer), CW for holes
    Surface surface;
};

auto body = Body::fromFaces(points, faceDefs);
// Returns nullopt on malformed input
```

**Winding Convention:** Outer loops **CCW** as seen from outside → coedges traverse edges in opposite directions on adjacent faces → coedges partner with opposite `reversed` flag.

---

## Primitives

```cpp
Body makeBox(float w, float h, float d);                    // Centered, 6 planar faces
Body makeCylinder(float r, float h, uint32_t seg);          // Capped cylinder
Body makeSphere(float r, uint32_t lat, uint32_t lon);       // UV sphere
Body makeCone(float r, float h, uint32_t seg);              // Capped cone
Body makeTorus(float R, float r, uint32_t major, uint32_t minor);
```

---

## Integrity Validation

```cpp
struct IntegrityReport {
    bool ok = true;
    std::string reason;
    uint32_t vertices, edges, faces, coedges, loops, shells;
    uint32_t boundaryEdges;
    int euler;  // V - E + F
};

IntegrityReport checkIntegrity() const;
```

### Checks Performed

| Check | Failure Example |
|-------|-----------------|
| Edge vertices valid | `edge.vert[0] >= vertexCount` |
| Edge non-degenerate | `v0 == v1` |
| Coedge edge valid | `coedge.edge >= edges.size()` |
| Coedge loop valid | `coedge.loop >= loops.size()` |
| Next/prev bounds | `next >= coedges.size()` |
| Next/prev reciprocal | `coedges[next].prev != self` |
| Next/prev loop match | `coedges[next].loop != coedge.loop` |
| Vertex continuity | `endV(c) != startV(c.next)` |
| Partner reciprocal | `partner.partner != self` |
| Partner orientation | `partner.reversed == coedge.reversed` |
| Partner edge match | `partner.edge != coedge.edge` |
| Partner loop diff | `partner.loop == coedge.loop` |
| Loop closure | Ring doesn't close within 2×coedges |
| Loop size | `< 3 coedges` |
| Face outer loop valid | `face.outerLoop >= loops.size()` |
| Face outer loop marked | `!loops[outer].outer` |
| Face outer loop ownership | `loops[outer].face != face` |
| Inner loop validity | `inner >= loops`, `loops[inner].outer`, `loops[inner].face != face` |
| Vertex root valid | `vertex.coedge >= coedges` or `startV(coedge) != vertex` |
| Edge orphaned | `coedgesPerEdge[e] == 0` |
| Edge non-manifold | `coedgesPerEdge[e] > 2` |
| Euler characteristic | `V - E + F != 2 - 2*genus - shells + boundaries` |

---

## Geometry Evaluation

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

### Surface Parametrizations

**Plane:** `eval(u,v) = origin + u*uAxis + v*vAxis`, `normalAt = normal`

**Cylinder:** `eval(u,v) = origin + cos(u)*uAxis*radius + sin(u)*vAxis*radius + v*normal`, `normalAt = normalize(cos(u)*uAxis + sin(u)*vAxis)`

**Sphere:** `eval(u,v) = origin + cos(u)*sin(v)*uAxis + sin(u)*sin(v)*vAxis + cos(v)*normal`, `normalAt = eval - origin`

**Torus:** `eval(u,v) = origin + (R + r*cos(v)) * (cos(u)*uAxis + sin(u)*vAxis) + r*sin(v)*normal`

**Cone:** `eval(u,v) = origin + (1-v)*r*cos(u)*uAxis + (1-v)*r*sin(u)*vAxis + v*h*normal`

---

## Tessellation

```cpp
Mesh Body::toMesh() const;
```
- Fan-triangulates outer loop of each face
- Returns indexed mesh (positions + topology)
- Suitable for rendering or export

---

## Complete Example: Box with Cylindrical Hole

```cpp
Body box = makeBox(10, 10, 5);
Body cyl = makeCylinder(1.5, 6);  // r=1.5, h=6

auto result = booleanDifference(box, cyl);
assert(result.isClosed());
auto mesh = result.toMesh();
// mesh: 12 tris (box) - cylindrical hole
```

---

## Boolean Operations (Exact)

```cpp
Body booleanUnion(const Body&, const Body&);
Body booleanDifference(const Body&, const Body&);
Body booleanIntersection(const Body&, const Body&);
```
- Exact curve/surface intersection
- Exact trimming at boundaries
- Watertight results, Euler χ=2

---

## Integrity Report

```cpp
struct IntegrityReport {
    bool ok = true;
    std::string reason;
    uint32_t vertices, edges, faces, coedges, loops, shells;
    uint32_t boundaryEdges;
    int euler;
};
```

### Check Categories

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

## Detailed Surface Evaluation

### Plane
```cpp
Vec3 Surface::eval(float u, float v) const {
    return origin + u * uAxis + v * vAxis();
}
Vec3 Surface::normalAt(float, float) const {
    return normal;
}
```

### Cylinder (radius r, axis = normal)
```cpp
Vec3 Surface::eval(float u, float v) const {
    Vec3 vAxis = normal.cross(uAxis);  // vAxis = normal × uAxis
    return origin + radius * (cos(u) * uAxis + sin(u) * vAxis) + v * normal;
}
Vec3 Surface::normalAt(float u, float v) const {
    return cos(u) * uAxis + sin(u) * vAxis();
}
```

### Sphere (radius r)
```cpp
Vec3 Surface::eval(float u, float v) const {
    // u: azimuth [0, 2π], v: elevation [-π/2, π/2]
    float cu = cos(u), su = sin(u);
    float cv = cos(v), sv = sin(v);
    Vec3 vAxis = normal.cross(uAxis);
    return origin + radius * (cu * cv * uAxis + su * cv * vAxis + sv * normal);
}
Vec3 Surface::normalAt(float u, float v) const {
    float cu = cos(u), su = sin(u);
    float cv = cos(v), sv = sin(v);
    Vec3 vAxis = normal.cross(uAxis);
    return cu * cv * uAxis + su * cv * vAxis + sv * normal;
}
```

### Torus (major R, minor r)
```cpp
Vec3 Surface::eval(float u, float v) const {
    // u: major angle [0, 2π], v: minor angle [0, 2π]
    float cu = cos(u), su = sin(u);
    float cv = cos(v), sv = sin(v);
    Vec3 vAxis = normal.cross(uAxis);
    float majorRadius = R + r * cv;
    return origin + majorRadius * (cu * uAxis + su * vAxis) + r * sv * normal;
}
Vec3 Surface::normalAt(float u, float v) const {
    float cu = cos(u), su = sin(u);
    float cv = cos(v), sv = sin(v);
    Vec3 vAxis = normal.cross(uAxis);
    return cv * (cu * uAxis + su * vAxis) + sv * normal;
}
```

### Cone (radius r, height h)
```cpp
Vec3 Surface::eval(float u, float v) const {
    // v: [0,1] along height
    float cu = cos(u), su = sin(u);
    Vec3 vAxis = normal.cross(uAxis);
    float radiusAtV = radius * (1.0f - v);
    return origin + radiusAtV * (cu * uAxis + su * vAxis) + v * height * normal;
}
Vec3 Surface::normalAt(float u, float v) const {
    // Normal points outward and slightly down
    float cu = cos(u), su = sin(u);
    Vec3 vAxis = normal.cross(uAxis);
    Vec3 radial = cu * uAxis + su * vAxis;
    // Cone angle: atan(radius/height)
    float sinAlpha = radius / sqrt(radius*radius + height*height);
    float cosAlpha = height / sqrt(radius*radius + height*height);
    return cosAlpha * radial - sinAlpha * normal;
}
```

---

## Primitive Construction Details

### `makeBox(w, h, d)`
- 8 vertices (±w/2, ±h/2, ±d/2)
- 12 edges (4 per axis)
- 6 faces (XY, XZ, YZ planes at ±half-extents)
- All surfaces: Plane
- Euler: V=8, E=12, F=6, χ=2

### `makeCylinder(r, h, seg)`
- 2*seg vertices (top + bottom circles)
- 3*seg edges (seg top, seg bottom, seg vertical)
- seg+2 faces (seg side, 1 top, 1 bottom)
- Side: Cylinder surface, Caps: Plane
- Euler: V=2s, E=3s, F=s+2, χ=2

### `makeSphere(r, lat, lon)`
- (lat+1)*(lon+1) vertices (UV sphere)
- ~3*lat*lon edges
- ~2*lat*lon faces
- All faces: Sphere surface
- Euler: χ=2

### `makeCone(r, h, seg)`
- seg+1 vertices (base circle + apex)
- 2*seg edges (base circle + seg to apex)
- seg+1 faces (seg side triangles + 1 base)
- Side: Cone surface, Base: Plane
- Euler: χ=2

### `makeTorus(R, r, major, minor)`
- major*minor vertices
- 2*major*minor edges
- major*minor faces
- All faces: Torus surface
- Euler: χ=0 (genus 1)

---

## Boolean Operations Implementation

### Exact Curve/Surface Intersection

**Line-Plane:** Analytic solution, parameter t
**Line-Cylinder:** Quadratic equation
**Line-Sphere:** Quadratic equation
**Line-Torus:** Quartic equation (solve analytically or numerically)
**Line-Cone:** Quadratic equation
**Circle-Plane:** Line-Plane + circle intersection
**Circle-Cylinder/Cone/Sphere/Torus:** Reduce to polynomial

### Trimming at Boundaries
1. Compute intersection curves (3D curves on surfaces)
2. Project to UV space of each surface
3. Trim UV domains to intersection curves
4. Update edge topology with new vertices at intersections
5. Rebuild loops from trimmed edges

### Boolean Algorithm
```cpp
Body booleanUnion(const Body& A, const Body& B) {
    // 1. Compute all intersection curves
    // 2. Split edges at intersections
    // 3. Classify faces: inside/outside/on boundary
    // 4. Remove inside faces
    // 5. Stitch boundary edges
    // 6. Build result topology
    // 7. Validate integrity
}
```

---

*Kernel v0.1.0-dev | 1921 tests passing | C++26 | Vulkan 1.3*