#pragma once

#include <vector>
#include <functional>
#include <optional>
#include <utility>
#include <cmath>
#include <algorithm>
#include <limits>
#include <map>
#include <nexus/utility/math/vector3_utils.h>
#include <nexus/utility/graphics/bounding_box.h>
#include <nexus/utility/graphics/convex_hull3d.h>

namespace nexus::utility::graphics {
using Vector3 = nexus::utility::math::Vector3;

/// @brief Convex collision queries: GJK intersection/distance, EPA penetration,
///        the Separating Axis Theorem, and ray casting against convex polyhedra.
///
/// GJK-family routines operate on abstract support functions (the furthest point
/// of a shape in a given direction), so any convex shape - primitive or point
/// cloud - can be tested. EPA recovers penetration depth and contact points when
/// shapes overlap. SAT works directly on convex polyhedra given as vertex sets.
class CollisionDetection {
public:
    using Vector3 = nexus::utility::math::Vector3;

    /// @brief A support mapping: returns the furthest point of a shape in @p dir.
    using SupportFunction = std::function<Vector3(const Vector3&)>;

    struct ClosestPoints {
        Vector3 onA;
        Vector3 onB;
    };

    // ── Support helpers ─────────────────────────────────────────────────

    static Vector3 supportSphere(const Vector3& center, float radius, const Vector3& dir) {
        Vector3 n = dir.normalized();
        if (n.lengthSquared() < 1e-12f) n = Vector3(1, 0, 0);
        return center + n * radius;
    }

    static Vector3 supportBox(const Vector3& boxMin, const Vector3& boxMax,
                              const Vector3& dir) {
        return Vector3(dir.x >= 0.0f ? boxMax.x : boxMin.x,
                       dir.y >= 0.0f ? boxMax.y : boxMin.y,
                       dir.z >= 0.0f ? boxMax.z : boxMin.z);
    }

    static Vector3 supportPolytope(const std::vector<Vector3>& verts, const Vector3& dir) {
        float best = -std::numeric_limits<float>::max();
        Vector3 result;
        for (const Vector3& v : verts) {
            float d = dot(v, dir);
            if (d > best) { best = d; result = v; }
        }
        return result;
    }

    static SupportFunction makeSphereSupport(const Vector3& center, float radius) {
        return [center, radius](const Vector3& d) { return supportSphere(center, radius, d); };
    }
    static SupportFunction makeBoxSupport(const Vector3& mn, const Vector3& mx) {
        return [mn, mx](const Vector3& d) { return supportBox(mn, mx, d); };
    }
    static SupportFunction makePolytopeSupport(std::vector<Vector3> verts) {
        return [verts = std::move(verts)](const Vector3& d) {
            return supportPolytope(verts, d);
        };
    }

    // ── GJK boolean intersection ────────────────────────────────────────

    /// @brief True if the two convex shapes intersect (Gilbert-Johnson-Keerthi).
    static bool gjkIntersect(const SupportFunction& shapeA, const SupportFunction& shapeB) {
        std::vector<SupportPoint> simplex;
        return gjk(shapeA, shapeB, simplex);
    }

    // ── GJK distance / EPA penetration ──────────────────────────────────

    /// @brief Distance between two convex shapes.
    ///
    /// Returns the positive separation when the shapes are disjoint, or the
    /// negative penetration depth (via EPA) when they overlap.
    static float gjkDistance(const SupportFunction& shapeA, const SupportFunction& shapeB) {
        Vector3 onA, onB;
        bool intersecting = false;
        float dist = gjkDistanceInternal(shapeA, shapeB, onA, onB, intersecting);
        if (!intersecting) return dist;
        EpaResult epa = runEPA(shapeA, shapeB);
        return -epa.depth;
    }

    /// @brief Closest points on each shape (or contact points when overlapping).
    static ClosestPoints gjkClosestPoints(const SupportFunction& shapeA,
                                          const SupportFunction& shapeB) {
        Vector3 onA, onB;
        bool intersecting = false;
        gjkDistanceInternal(shapeA, shapeB, onA, onB, intersecting);
        if (!intersecting) return {onA, onB};
        EpaResult epa = runEPA(shapeA, shapeB);
        return {epa.onA, epa.onB};
    }

    // ── Separating Axis Theorem ─────────────────────────────────────────

    /// @brief Candidate face-normal axes of a convex vertex set.
    ///
    /// Approximates the shape's face normals as normalized cross-products of its
    /// edge directions (pairwise vertex differences), de-duplicated.
    static std::vector<Vector3> satAxes(const std::vector<Vector3>& vertices) {
        std::vector<Vector3> edges = uniqueEdgeDirs(vertices);
        std::vector<Vector3> axes;
        for (size_t i = 0; i < edges.size(); ++i)
            for (size_t j = i + 1; j < edges.size(); ++j) {
                Vector3 axis = cross(edges[i], edges[j]);
                addUnique(axes, axis);
            }
        return axes;
    }

    /// @brief True if two convex polyhedra (given by vertices) intersect.
    static bool satIntersect(const std::vector<Vector3>& verticesA,
                             const std::vector<Vector3>& verticesB) {
        if (verticesA.empty() || verticesB.empty()) return false;

        std::vector<Vector3> axes = satAxes(verticesA);
        for (const Vector3& a : satAxes(verticesB)) addUnique(axes, a);

        // Edge-edge cross-product axes.
        std::vector<Vector3> edgesA = uniqueEdgeDirs(verticesA);
        std::vector<Vector3> edgesB = uniqueEdgeDirs(verticesB);
        for (const Vector3& ea : edgesA)
            for (const Vector3& eb : edgesB)
                addUnique(axes, cross(ea, eb));

        for (const Vector3& axis : axes) {
            if (axis.lengthSquared() < 1e-12f) continue;
            if (separatedOnAxis(verticesA, verticesB, axis)) return false;
        }
        return true;
    }

    // ── Ray casting ─────────────────────────────────────────────────────

    /// @brief Cast a ray against a convex polyhedron (given by its vertices).
    /// @return the entry parameter t (>= 0) if the ray hits, otherwise nullopt.
    static std::optional<float> rayCast(const Ray& ray, const std::vector<Vector3>& vertices) {
        if (vertices.size() < 4) return std::nullopt;

        ConvexHull3D hull;
        ConvexHull3D::HullResult hr = hull.computeHull(vertices);
        if (hr.faceIndices.size() < 3) return std::nullopt;

        // Centroid to orient each face plane's normal outward.
        Vector3 centroid(0, 0, 0);
        for (const Vector3& v : hr.vertices) centroid += v;
        centroid = centroid / static_cast<float>(hr.vertices.size());

        float tEnter = 0.0f;
        float tExit = std::numeric_limits<float>::max();
        const Vector3& o = ray.origin;
        const Vector3& d = ray.direction;

        const size_t faceCount = hr.faceIndices.size() / 3;
        for (size_t f = 0; f < faceCount; ++f) {
            const Vector3& a = hr.vertices[hr.faceIndices[f * 3 + 0]];
            const Vector3& b = hr.vertices[hr.faceIndices[f * 3 + 1]];
            const Vector3& c = hr.vertices[hr.faceIndices[f * 3 + 2]];
            Vector3 n = cross(b - a, c - a);
            float len = n.length();
            if (len < 1e-12f) continue;
            n = n / len;
            if (dot(n, centroid - a) > 0.0f) n = n * -1.0f; // ensure outward
            // Half-space: dot(n, x - a) <= 0 is inside.
            float denom = dot(n, d);
            float dist = dot(n, a - o); // dot(n, x-o)=0 at plane
            if (std::abs(denom) < 1e-12f) {
                if (dot(n, o - a) > 1e-6f) return std::nullopt; // parallel & outside
                continue;
            }
            float t = dist / denom;
            if (denom < 0.0f) {
                if (t > tEnter) tEnter = t;
            } else {
                if (t < tExit) tExit = t;
            }
            if (tEnter > tExit) return std::nullopt;
        }
        if (tEnter <= tExit && tExit >= 0.0f) return tEnter;
        return std::nullopt;
    }

private:
    struct SupportPoint {
        Vector3 v; // point on the Minkowski difference (A - B)
        Vector3 a; // support point on A
        Vector3 b; // support point on B
    };

    static float dot(const Vector3& a, const Vector3& b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }
    static Vector3 cross(const Vector3& a, const Vector3& b) {
        return {a.y * b.z - a.z * b.y,
                a.z * b.x - a.x * b.z,
                a.x * b.y - a.y * b.x};
    }
    static bool sameDir(const Vector3& a, const Vector3& b) { return dot(a, b) > 0.0f; }

    static SupportPoint support(const SupportFunction& A, const SupportFunction& B,
                                const Vector3& dir) {
        SupportPoint sp;
        sp.a = A(dir);
        sp.b = B(dir * -1.0f);
        sp.v = sp.a - sp.b;
        return sp;
    }

    // ─── GJK boolean simplex machinery ─────────────────────────────────

    static bool gjk(const SupportFunction& A, const SupportFunction& B,
                    std::vector<SupportPoint>& simplex) {
        Vector3 dir(1, 0, 0);
        simplex.clear();
        simplex.insert(simplex.begin(), support(A, B, dir));
        dir = simplex[0].v * -1.0f;

        for (int iter = 0; iter < 64; ++iter) {
            if (dir.lengthSquared() < 1e-12f) dir = Vector3(1, 0, 0);
            SupportPoint p = support(A, B, dir);
            if (dot(p.v, dir) < 0.0f) return false; // no intersection
            simplex.insert(simplex.begin(), p);
            if (nextSimplex(simplex, dir)) return true;
        }
        return false;
    }

    static bool nextSimplex(std::vector<SupportPoint>& s, Vector3& dir) {
        switch (s.size()) {
            case 2: return lineCase(s, dir);
            case 3: return triangleCase(s, dir);
            case 4: return tetraCase(s, dir);
            default: return false;
        }
    }

    static bool lineCase(std::vector<SupportPoint>& s, Vector3& dir) {
        const Vector3& a = s[0].v;
        const Vector3& b = s[1].v;
        Vector3 ab = b - a;
        Vector3 ao = a * -1.0f;
        if (sameDir(ab, ao)) {
            dir = cross(cross(ab, ao), ab);
        } else {
            s = {s[0]};
            dir = ao;
        }
        return false;
    }

    static bool triangleCase(std::vector<SupportPoint>& s, Vector3& dir) {
        const Vector3& a = s[0].v;
        const Vector3& b = s[1].v;
        const Vector3& c = s[2].v;
        Vector3 ab = b - a, ac = c - a, ao = a * -1.0f;
        Vector3 abc = cross(ab, ac);

        if (sameDir(cross(abc, ac), ao)) {
            if (sameDir(ac, ao)) {
                s = {s[0], s[2]};
                dir = cross(cross(ac, ao), ac);
            } else {
                s = {s[0], s[1]};
                return lineCase(s, dir);
            }
        } else {
            if (sameDir(cross(ab, abc), ao)) {
                s = {s[0], s[1]};
                return lineCase(s, dir);
            } else {
                if (sameDir(abc, ao)) {
                    dir = abc;
                } else {
                    s = {s[0], s[2], s[1]};
                    dir = abc * -1.0f;
                }
            }
        }
        return false;
    }

    static bool tetraCase(std::vector<SupportPoint>& s, Vector3& dir) {
        const Vector3& a = s[0].v;
        const Vector3& b = s[1].v;
        const Vector3& c = s[2].v;
        const Vector3& d = s[3].v;
        Vector3 ab = b - a, ac = c - a, ad = d - a, ao = a * -1.0f;
        Vector3 abc = cross(ab, ac);
        Vector3 acd = cross(ac, ad);
        Vector3 adb = cross(ad, ab);

        if (sameDir(abc, ao)) { s = {s[0], s[1], s[2]}; return triangleCase(s, dir); }
        if (sameDir(acd, ao)) { s = {s[0], s[2], s[3]}; return triangleCase(s, dir); }
        if (sameDir(adb, ao)) { s = {s[0], s[3], s[1]}; return triangleCase(s, dir); }
        return true; // origin enclosed
    }

    // ─── GJK distance (closest points) ─────────────────────────────────

    struct Closest {
        Vector3 point;
        std::vector<SupportPoint> pts;
        std::vector<float> w;
    };

    static float gjkDistanceInternal(const SupportFunction& A, const SupportFunction& B,
                                     Vector3& onA, Vector3& onB, bool& intersecting) {
        intersecting = false;
        std::vector<SupportPoint> simplex;
        Vector3 dir(1, 0, 0);
        simplex.push_back(support(A, B, dir));
        Closest closest;
        closest.point = simplex[0].v;
        closest.pts = simplex;
        closest.w = {1.0f};
        dir = simplex[0].v * -1.0f;
        float prevDist = std::numeric_limits<float>::max();

        for (int iter = 0; iter < 64; ++iter) {
            if (dir.lengthSquared() < 1e-14f) { intersecting = true; break; }
            SupportPoint p = support(A, B, dir);

            bool dup = false;
            for (const SupportPoint& e : simplex)
                if ((e.v - p.v).lengthSquared() < 1e-12f) { dup = true; break; }
            if (dup) break;

            simplex.push_back(p);
            closest = closestToOrigin(simplex);
            simplex = closest.pts;

            float dist = closest.point.length();
            if (dist < 1e-7f) { intersecting = true; break; }
            if (simplex.size() == 4) { intersecting = true; break; }

            dir = closest.point * -1.0f;
            if (prevDist - dist < 1e-8f) break; // converged
            prevDist = dist;
        }

        onA = Vector3(); onB = Vector3();
        for (size_t i = 0; i < closest.pts.size(); ++i) {
            onA += closest.pts[i].a * closest.w[i];
            onB += closest.pts[i].b * closest.w[i];
        }
        return closest.point.length();
    }

    static Closest closestToOrigin(const std::vector<SupportPoint>& s) {
        switch (s.size()) {
            case 1: return {s[0].v, {s[0]}, {1.0f}};
            case 2: return closestSegment(s[0], s[1]);
            case 3: return closestTriangle(s[0], s[1], s[2]);
            default: return closestTetra(s[0], s[1], s[2], s[3]);
        }
    }

    static Closest closestSegment(const SupportPoint& s0, const SupportPoint& s1) {
        Vector3 ab = s1.v - s0.v;
        float denom = dot(ab, ab);
        if (denom < 1e-14f) return {s0.v, {s0}, {1.0f}};
        float t = dot(s0.v * -1.0f, ab) / denom;
        if (t <= 0.0f) return {s0.v, {s0}, {1.0f}};
        if (t >= 1.0f) return {s1.v, {s1}, {1.0f}};
        Vector3 c = s0.v + ab * t;
        return {c, {s0, s1}, {1.0f - t, t}};
    }

    static void triangleBary(const Vector3& A, const Vector3& B, const Vector3& C,
                             float& l0, float& l1, float& l2) {
        // Ericson closest-point-in-triangle to the origin.
        Vector3 ab = B - A, ac = C - A, ap = A * -1.0f;
        float d1 = dot(ab, ap), d2 = dot(ac, ap);
        if (d1 <= 0.0f && d2 <= 0.0f) { l0 = 1; l1 = 0; l2 = 0; return; }
        Vector3 bp = B * -1.0f;
        float d3 = dot(ab, bp), d4 = dot(ac, bp);
        if (d3 >= 0.0f && d4 <= d3) { l0 = 0; l1 = 1; l2 = 0; return; }
        float vc = d1 * d4 - d3 * d2;
        if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
            float v = d1 / (d1 - d3);
            l0 = 1 - v; l1 = v; l2 = 0; return;
        }
        Vector3 cp = C * -1.0f;
        float d5 = dot(ab, cp), d6 = dot(ac, cp);
        if (d6 >= 0.0f && d5 <= d6) { l0 = 0; l1 = 0; l2 = 1; return; }
        float vb = d5 * d2 - d1 * d6;
        if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
            float w = d2 / (d2 - d6);
            l0 = 1 - w; l1 = 0; l2 = w; return;
        }
        float va = d3 * d6 - d5 * d4;
        if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
            float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
            l0 = 0; l1 = 1 - w; l2 = w; return;
        }
        float denom = 1.0f / (va + vb + vc);
        float v = vb * denom, w = vc * denom;
        l0 = 1 - v - w; l1 = v; l2 = w;
    }

    static Closest closestTriangle(const SupportPoint& s0, const SupportPoint& s1,
                                   const SupportPoint& s2) {
        float l0, l1, l2;
        triangleBary(s0.v, s1.v, s2.v, l0, l1, l2);
        Vector3 c = s0.v * l0 + s1.v * l1 + s2.v * l2;
        std::vector<SupportPoint> pts;
        std::vector<float> w;
        if (l0 > 1e-7f) { pts.push_back(s0); w.push_back(l0); }
        if (l1 > 1e-7f) { pts.push_back(s1); w.push_back(l1); }
        if (l2 > 1e-7f) { pts.push_back(s2); w.push_back(l2); }
        normalizeWeights(w);
        if (pts.empty()) { pts.push_back(s0); w.push_back(1.0f); }
        return {c, pts, w};
    }

    static bool pointOutsidePlane(const Vector3& A, const Vector3& B,
                                  const Vector3& C, const Vector3& D) {
        Vector3 n = cross(B - A, C - A);
        float signp = dot(A * -1.0f, n);   // dot(origin - A, n)
        float signd = dot(D - A, n);
        return signp * signd < 0.0f;
    }

    static Closest closestTetra(const SupportPoint& s0, const SupportPoint& s1,
                                const SupportPoint& s2, const SupportPoint& s3) {
        Closest best;
        best.point = Vector3();
        float bestSq = std::numeric_limits<float>::max();
        bool found = false;

        auto tryFace = [&](const SupportPoint& a, const SupportPoint& b,
                           const SupportPoint& c, const SupportPoint& d) {
            if (!pointOutsidePlane(a.v, b.v, c.v, d.v)) return;
            Closest cand = closestTriangle(a, b, c);
            float sq = cand.point.lengthSquared();
            if (sq < bestSq) { bestSq = sq; best = cand; found = true; }
        };

        tryFace(s0, s1, s2, s3);
        tryFace(s0, s2, s3, s1);
        tryFace(s0, s3, s1, s2);
        tryFace(s1, s3, s2, s0);

        if (!found) {
            // Origin inside the tetrahedron.
            return {Vector3(), {s0, s1, s2, s3}, {0.25f, 0.25f, 0.25f, 0.25f}};
        }
        return best;
    }

    static void normalizeWeights(std::vector<float>& w) {
        float sum = 0.0f;
        for (float x : w) sum += x;
        if (sum > 1e-12f) for (float& x : w) x /= sum;
    }

    // ─── EPA (Expanding Polytope Algorithm) ────────────────────────────

    struct EpaResult {
        float depth = 0.0f;
        Vector3 normal;
        Vector3 onA;
        Vector3 onB;
    };

    struct EpaFace {
        int a, b, c;
        Vector3 normal;
        float dist;
    };

    static void makeEpaFace(const std::vector<SupportPoint>& verts,
                            int a, int b, int c, std::vector<EpaFace>& faces) {
        Vector3 n = cross(verts[b].v - verts[a].v, verts[c].v - verts[a].v);
        float len = n.length();
        if (len < 1e-12f) return;
        n = n / len;
        float d = dot(n, verts[a].v);
        if (d < 0.0f) { n = n * -1.0f; d = -d; std::swap(b, c); }
        faces.push_back({a, b, c, n, d});
    }

    static EpaResult runEPA(const SupportFunction& A, const SupportFunction& B) {
        EpaResult result;
        std::vector<SupportPoint> simplex;
        if (!gjk(A, B, simplex) || simplex.size() < 4) return result;

        std::vector<SupportPoint> verts = simplex;
        std::vector<EpaFace> faces;
        makeEpaFace(verts, 0, 1, 2, faces);
        makeEpaFace(verts, 0, 2, 3, faces);
        makeEpaFace(verts, 0, 3, 1, faces);
        makeEpaFace(verts, 1, 3, 2, faces);
        if (faces.empty()) return result;

        for (int iter = 0; iter < 64; ++iter) {
            // Closest face to the origin.
            int minIdx = 0;
            float minDist = faces[0].dist;
            for (int i = 1; i < static_cast<int>(faces.size()); ++i)
                if (faces[i].dist < minDist) { minDist = faces[i].dist; minIdx = i; }

            Vector3 dir = faces[minIdx].normal;
            SupportPoint p = support(A, B, dir);
            float d = dot(p.v, dir);

            if (d - minDist < 1e-5f) {
                result.depth = minDist;
                result.normal = dir;
                contactFromFace(verts, faces[minIdx], result.onA, result.onB);
                return result;
            }

            int newIdx = static_cast<int>(verts.size());
            verts.push_back(p);

            // Remove faces visible from p; collect horizon edges (used once).
            std::map<std::pair<int, int>, int> edgeCount;
            std::vector<EpaFace> keep;
            for (const EpaFace& f : faces) {
                if (dot(f.normal, p.v - verts[f.a].v) > 1e-9f) {
                    addEpaEdge(edgeCount, f.a, f.b);
                    addEpaEdge(edgeCount, f.b, f.c);
                    addEpaEdge(edgeCount, f.c, f.a);
                } else {
                    keep.push_back(f);
                }
            }
            faces.swap(keep);
            for (const auto& kv : edgeCount) {
                if (kv.second != 1) continue;
                makeEpaFace(verts, kv.first.first, kv.first.second, newIdx, faces);
            }
            if (faces.empty()) break;
        }

        // Fallback: return the best face found so far.
        int minIdx = 0;
        float minDist = faces.empty() ? 0.0f : faces[0].dist;
        for (int i = 1; i < static_cast<int>(faces.size()); ++i)
            if (faces[i].dist < minDist) { minDist = faces[i].dist; minIdx = i; }
        if (!faces.empty()) {
            result.depth = minDist;
            result.normal = faces[minIdx].normal;
            contactFromFace(verts, faces[minIdx], result.onA, result.onB);
        }
        return result;
    }

    static void addEpaEdge(std::map<std::pair<int, int>, int>& m, int a, int b) {
        if (a > b) std::swap(a, b);
        ++m[{a, b}];
    }

    static void contactFromFace(const std::vector<SupportPoint>& verts,
                                const EpaFace& f, Vector3& onA, Vector3& onB) {
        // Project origin onto the face plane, then barycentric-interpolate.
        Vector3 proj = f.normal * f.dist;
        const Vector3& A = verts[f.a].v;
        const Vector3& B = verts[f.b].v;
        const Vector3& C = verts[f.c].v;
        float u, v, w;
        barycentric(proj, A, B, C, u, v, w);
        onA = verts[f.a].a * u + verts[f.b].a * v + verts[f.c].a * w;
        onB = verts[f.a].b * u + verts[f.b].b * v + verts[f.c].b * w;
    }

    static void barycentric(const Vector3& p, const Vector3& a, const Vector3& b,
                            const Vector3& c, float& u, float& v, float& w) {
        Vector3 v0 = b - a, v1 = c - a, v2 = p - a;
        float d00 = dot(v0, v0), d01 = dot(v0, v1), d11 = dot(v1, v1);
        float d20 = dot(v2, v0), d21 = dot(v2, v1);
        float denom = d00 * d11 - d01 * d01;
        if (std::abs(denom) < 1e-12f) { u = 1; v = 0; w = 0; return; }
        v = (d11 * d20 - d01 * d21) / denom;
        w = (d00 * d21 - d01 * d20) / denom;
        u = 1.0f - v - w;
    }

    // ─── SAT helpers ───────────────────────────────────────────────────

    static std::vector<Vector3> uniqueEdgeDirs(const std::vector<Vector3>& verts) {
        std::vector<Vector3> dirs;
        for (size_t i = 0; i < verts.size(); ++i)
            for (size_t j = i + 1; j < verts.size(); ++j) {
                Vector3 e = verts[j] - verts[i];
                float len = e.length();
                if (len < 1e-8f) continue;
                addUnique(dirs, e / len);
            }
        return dirs;
    }

    static void addUnique(std::vector<Vector3>& axes, const Vector3& axis) {
        float len = axis.length();
        if (len < 1e-8f) return;
        Vector3 n = axis / len;
        for (const Vector3& existing : axes) {
            float d = std::abs(dot(existing, n));
            if (d > 0.9999f) return; // parallel axis already present
        }
        axes.push_back(n);
    }

    static bool separatedOnAxis(const std::vector<Vector3>& A,
                                const std::vector<Vector3>& B, const Vector3& axis) {
        float minA = std::numeric_limits<float>::max();
        float maxA = std::numeric_limits<float>::lowest();
        float minB = std::numeric_limits<float>::max();
        float maxB = std::numeric_limits<float>::lowest();
        for (const Vector3& v : A) { float p = dot(v, axis); minA = std::min(minA, p); maxA = std::max(maxA, p); }
        for (const Vector3& v : B) { float p = dot(v, axis); minB = std::min(minB, p); maxB = std::max(maxB, p); }
        const float eps = 1e-6f;
        return maxA < minB - eps || maxB < minA - eps;
    }
};

} // namespace nexus::utility::graphics
