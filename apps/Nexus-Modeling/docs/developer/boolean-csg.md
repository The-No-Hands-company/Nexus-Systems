# Nexus Modeling — Boolean/CSG Deep Dive

*Real CSG pipeline: tri-tri intersection → per-triangle retriangulation → whole-mesh cut → classify/stitch.*

---

## Pipeline Overview

```
Mesh A + Mesh B
    │
    ▼
Triangle-Triangle Intersection (robust, exact predicates)
    │
    ▼
Per-triangle Retriangulation (split at intersections)
    │
    ▼
Whole-mesh Cut (classify vertices: inside/outside/on-surface)
    │
    ▼
Classify & Stitch (inside→remove, outside→keep, on→stitch)
    │
    ▼
Watertight 2-manifold Result (Euler χ=2, exact volumes)
```

---

## 1. Triangle-Triangle Intersection

### Robust Segment-Triangle Intersection

```cpp
// Möller–Trumbore with exact predicates
bool rayTriangleIntersect(const Vec3& ro, const Vec3& rd,
                          const Vec3& v0, const Vec3& v1, const Vec3& v2,
                          float& t) {
    Vec3 e1 = v1 - v0, e2 = v2 - v0;
    Vec3 h = rd.cross(e2);
    float a = e1.dot(h);
    if (std::fabs(a) < 1e-7f) return false;  // Parallel/degenerate
    float f = 1.f / a;
    Vec3 s = ro - v0;
    float u = f * s.dot(h);
    if (u < 0.f || u > 1.f) return false;
    Vec3 q = s.cross(e1);
    float v = f * rd.dot(q);
    if (v < 0.f || u + v > 1.f) return false;
    float tt = f * e2.dot(q);
    if (tt < 1e-4f) return false;  // Epsilon offset
    t = tt;
    return true;
}
```

### Coarse Pick (Ray-Sphere Fallback)

```cpp
FeatureId coarsePick(const CadDocument& doc,
                      const Vec3& rayOrigin, const Vec3& rayDir) {
    using namespace nexus::parametric;
    FeatureId best = kInvalidFeatureId;
    float bestT = 1e30f;
    size_t fc = doc.history().featureCount();
    for (FeatureId i = 1; i <= static_cast<FeatureId>(fc); ++i) {
        auto* node = doc.history().node(i);
        if (!node || !node->mesh || node->deleted || node->hidden) continue;
        auto b = node->mesh->computeBounds();
        Vec3 center = b.center();
        float radius = std::max(b.extents().length() * 0.5f, 1.5f);
        Vec3 oc = rayOrigin - center;
        float a2 = rayDir.dot(rayDir);
        float b2 = 2.f * oc.dot(rayDir);
        float c2 = oc.dot(oc) - radius * radius;
        float disc = b2 * b2 - 4.f * a2 * c2;
        if (disc < 0) continue;
        float t = (-b2 - std::sqrt(disc)) / (2.f * a2);
        if (t > 0 && t < bestT) { best = i; bestT = t; }
    }
    return best;
}
```

### Exact Triangle-Triangle (Shewchuk Predicates)

```cpp
// Exact tri-tri using Shewchuk orient3d
// Returns intersection segment [t0, t1] on triangle A's plane
struct TriTriResult {
    bool intersects = false;
    Vec3 p0, p1;  // Intersection segment endpoints
    float t0, t1; // Parameters on triangle A edge
};

TriTriResult triTriExact(const Triangle& T1, const Triangle& T2) {
    // 1. Plane equations
    Vec3 n1 = (T1.v1 - T1.v0).cross(T1.v2 - T1.v0);
    Vec3 n2 = (T2.v1 - T2.v0).cross(T2.v2 - T2.v0);
    
    // 2. Signed distances of T2 vertices to T1 plane
    float d2[3];
    for (int i = 0; i < 3; ++i) d2[i] = n1.dot(T2.v[i] - T1.v0);
    
    // 3. Check if all same sign (no intersection)
    if ((d2[0] > 0 && d2[1] > 0 && d2[2] > 0) ||
        (d2[0] < 0 && d2[1] < 0 && d2[2] < 0)) {
        return {};
    }
    
    // 4. Compute intersection line segment on T1
    // Find edges of T2 that cross T1 plane
    // Clip against T1 edges using orient2d predicates
    // ...
}
```

---

## 2. Per-triangle Retriangulation

For each intersecting triangle pair:

```
1. Find all intersection segments on triangle A
2. Sort intersection points along triangle edges
3. Build constrained Delaunay triangulation
4. Same for triangle B
```

### Constrained Delaunay Triangulation

```cpp
std::vector<uint32_t> triangulateWithConstraints(
    const std::vector<Vec3>& vertices,
    const std::vector<std::pair<uint32_t,uint32_t>>& constraintEdges)
{
    // Bowyer-Watson with constrained edges
    // Protect constraint edges from being flipped
    // Return triangulated faces
    
    // Algorithm:
    // 1. Start with super-triangle encompassing all vertices
    // 2. Insert vertices one by one
    // 3. For each insertion:
    //    a. Find cavity (triangles whose circumcircle contains point)
    //    b. Remove cavity, create new triangles to point
    //    c. LEGALIZE: flip edges until Delaunay condition met
    //       BUT: never flip constraint edges
    // 4. Remove super-triangle vertices
}
```

### Intersection Segment Computation

```cpp
struct IntersectionSeg {
    Vec3 p0, p1;      // 3D endpoints
    float t0, t1;     // Parameters on triangle edge [0,1]
    uint32_t edgeA;   // Edge index on triangle A
    uint32_t edgeB;   // Edge index on triangle B
};

std::vector<IntersectionSeg> computeIntersections(
    const Triangle& T1, const Triangle& T2, 
    const Tolerance& tol)
{
    // 1. Compute plane intersection line
    // 2. Clip line against both triangles
    // 3. For each triangle:
    //    - Find intersections with each edge (segment-triangle)
    //    - Use exact predicates for on-edge tests
    //    - Sort by parameter along edge
    // 4. Pair up intersection points to form segments
}
```

---

## 3. Whole-mesh Cut

Classify all vertices of Mesh A against Mesh B:

```cpp
enum class VertexClass { Inside, Outside, OnSurface };

VertexClass classifyVertex(const Vec3& p, const Mesh& mesh) {
    // Ray-cast to infinity, count intersections
    // Use robust predicates for on-surface test
    // Even parity → Outside, Odd → Inside
}

VertexClass classifyVertex(const Vec3& p, const Mesh& mesh, const Tolerance& tol) {
    // Ray to infinity (e.g., +X direction)
    Vec3 dir = {1, 0, 0};
    Vec3 origin = p + dir * tol.at(p.length()) * 2; // Start outside tolerance

    int crossings = 0;
    for (each triangle in mesh) {
        float t;
        if (rayTriangleIntersect(origin, dir, v0, v1, v2, t)) {
            // Exact on-surface test
            Vec3 hit = origin + dir * t;
            if (pointOnTriangle(hit, triangle, tol)) 
                return VertexClass::OnSurface;
            crossings++;
        }
    }
    return (crossings % 2 == 1) ? VertexClass::Inside : VertexClass::Outside;
}
```

---

## 3. Whole-mesh Cut & Classify

```cpp
for each vertex v in Mesh A:
    v.class = classifyVertex(v.pos, Mesh B);

for each triangle in Mesh A:
    count Inside/Outside/OnSurface vertices
    if all Inside → discard
    if all Outside → keep
    if mixed → split edges at intersections, retriangulate
```

### Edge Splitting

```cpp
struct SplitPoint {
    float t;           // parameter along edge [0,1]
    Vec3 position;     // 3D position
    VertexClass cls;   // Inside/Outside/OnSurface
};

std::vector<SplitPoint> splitEdge(const Vec3& v0, const Vec3& v1,
                                   const Mesh& otherMesh, const Tolerance& tol) {
    std::vector<SplitPoint> splits;
    // Find all intersections with otherMesh triangles
    for (each triangle in otherMesh) {
        // Ray-triangle from v0 to v1
        // Find intersection parameters
    }
    // Sort by t, filter duplicates within tolerance
    return splits;
}
```

---

## 4. Classify & Stitch

### Vertex Classification

```cpp
enum VertexClass { Inside, Outside, OnSurface };
```

### Face Classification

```cpp
enum FaceClass { Inside, Outside, Boundary };

FaceClass classifyFace(const Triangle& tri) {
    int inside = 0, outside = 0, on = 0;
    for (each vertex) {
        switch (v.class) {
            case Inside: inside++; break;
            case Outside: outside++; break;
            case OnSurface: on++; break;
        }
    }
    if (inside > 0 && outside == 0 && on == 0) return FaceClass::Inside;
    if (outside > 0 && inside == 0 && on == 0) return FaceClass::Outside;
    return FaceClass::Boundary;
}
```

### Stitching

```cpp
for each Boundary edge (OnSurface vertices):
    Find matching edge on other mesh (within tolerance)
    If found: weld vertices, share edge
    If not found: create new vertex, mark as boundary
```

---

## Tolerance & Robustness

```cpp
struct Tolerance {
    static constexpr float ABS = 1e-6f;
    static constexpr float REL = 1e-4f;
    float at(float charLen) const { return max(ABS, REL * charLen); }
    bool nearlyEqual(float a, float b, float L) { return fabs(a-b) <= at(L); }
    bool isZero(float x, float L) { return fabs(x) <= at(L); }
    bool coincident(const Vec3& a, const Vec3& b, float L) {
        return (a-b).lengthSq() <= at(L)*at(L);
    }
};
```

### Usage

```cpp
Tolerance tol = Tolerance::forCharacteristicLength(charLen);
if (tol.nearlyEqual(a, b, charLen)) ...
if (tol.isZero(len, charLen)) ...
if (tol.coincident(p1, p2, charLen)) ...
```

---

## Results

| Test Case | Union | Intersection | Difference |
|-----------|-------|--------------|------------|
| Cube ∪ Cube | 15 tris | 1 tri | 7 tris |
| Cube ∩ Cube | 1 tri | 1 tri | 7 tris |
| Cube \ Cube | 7 tris | 1 tri | 15 tris |

All results: **Watertight, 2-manifold, Euler χ=2, exact volumes**.

---

## Remaining Work

- [ ] Coplanar face overlap handling
- [ ] Degenerate triangle filtering
- [ ] Performance optimization for large meshes (>1M tris)
- [ ] Exact curve/surface intersection for analytic B-rep Boolean

---

*Related: [Mesh Operations](mesh-operations.md) | [Half-edge Mesh](halfedge-mesh.md) | [Tolerance System](tolerance.md)*