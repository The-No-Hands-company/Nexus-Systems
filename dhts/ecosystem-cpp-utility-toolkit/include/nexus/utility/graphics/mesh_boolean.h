#pragma once

#include <vector>
#include <array>
#include <cstdint>
#include <cmath>
#include <utility>
#include <algorithm>
#include <unordered_map>
#include <nexus/utility/math/vector3_utils.h>

namespace nexus::utility::graphics {
using Vector3 = nexus::utility::math::Vector3;

/// @brief Constructive Solid Geometry (CSG) boolean operations on triangle meshes.
///
/// The mesh is decomposed into a set of planar convex polygon fragments and a
/// Binary Space Partitioning (BSP) tree is built from one operand. Each polygon
/// of the other operand is then pushed down the tree and split against the node
/// planes until every fragment lands in a homogeneous region (fully inside or
/// fully outside the solid). Union, intersection and difference are recombined
/// from the classified fragments following the standard Naylor/Thibault scheme:
///   union         = A\B  +  B\A
///   intersection  = A∩B (from A) + B∩A (from B)
///   difference    = A\B  +  flip(B∩A)
/// The result is fan-triangulated and welded into an indexed triangle mesh.
class MeshBoolean {
public:
    using Vector3 = nexus::utility::math::Vector3;

    enum Operation { Union, Intersection, Difference };
    enum Classification { IN, OUT, ON };

    /// @brief Oriented plane: dot(normal, x) = w.
    struct Plane {
        Vector3 normal{0.0f, 0.0f, 1.0f};
        float w = 0.0f;
        bool valid = false;
    };

    /// @brief A planar convex polygon fragment (a triangle after triangulation).
    struct Polygon {
        std::vector<Vector3> verts;
        Plane plane;
    };

    /// @brief A BSP node. `triangles` holds indices of coplanar polygons in the
    /// owning tree's polygon pool; front/back partition the two half-spaces.
    struct BSPNode {
        Plane splitPlane;
        std::vector<unsigned> triangles;
        BSPNode* front = nullptr;
        BSPNode* back = nullptr;
    };

    /// @brief Owning BSP tree: pool holds every polygon fragment created.
    struct BSPTree {
        BSPNode* root = nullptr;
        std::vector<Polygon> pool;

        BSPTree() = default;
        BSPTree(const BSPTree&) = delete;
        BSPTree& operator=(const BSPTree&) = delete;
        BSPTree(BSPTree&& o) noexcept
            : root(o.root), pool(std::move(o.pool)) { o.root = nullptr; }
        BSPTree& operator=(BSPTree&& o) noexcept {
            if (this != &o) {
                destroy(root);
                root = o.root;
                pool = std::move(o.pool);
                o.root = nullptr;
            }
            return *this;
        }
        ~BSPTree() { destroy(root); }

        static void destroy(BSPNode* n) {
            if (!n) return;
            destroy(n->front);
            destroy(n->back);
            delete n;
        }
    };

    /// @brief Indexed triangle mesh result.
    struct Result {
        std::vector<Vector3> verts;
        std::vector<unsigned> indices;
    };

    // ── Plane / polygon helpers ─────────────────────────────────────────

    /// @brief Build a plane from three triangle vertices (CCW winding).
    static Plane makePlane(const Vector3& a, const Vector3& b, const Vector3& c) {
        Plane p;
        Vector3 n = cross(b - a, c - a);
        float len = n.length();
        if (len < 1e-12f) { p.valid = false; return p; }
        p.normal = n / len;
        p.w = dot(p.normal, a);
        p.valid = true;
        return p;
    }

    /// @brief Reverse a polygon's orientation (flip winding and plane).
    static void flipPolygon(Polygon& poly) {
        std::reverse(poly.verts.begin(), poly.verts.end());
        poly.plane.normal = poly.plane.normal * -1.0f;
        poly.plane.w = -poly.plane.w;
    }

    /// @brief Convert an indexed triangle mesh into a list of polygon fragments.
    static std::vector<Polygon> meshToPolys(const std::vector<Vector3>& verts,
                                            const std::vector<unsigned>& indices) {
        std::vector<Polygon> polys;
        polys.reserve(indices.size() / 3);
        for (size_t t = 0; t + 2 < indices.size(); t += 3) {
            Polygon p;
            p.verts = {verts[indices[t]], verts[indices[t + 1]], verts[indices[t + 2]]};
            p.plane = makePlane(p.verts[0], p.verts[1], p.verts[2]);
            if (!p.plane.valid) continue;
            polys.push_back(std::move(p));
        }
        return polys;
    }

    /// @brief Split a polygon by a plane into coplanar/front/back fragments.
    ///
    /// Spanning polygons are cut and the two resulting convex pieces retain the
    /// source polygon's supporting plane.
    static void splitPolygon(const Plane& plane, const Polygon& poly,
                             std::vector<Polygon>& coplanarFront,
                             std::vector<Polygon>& coplanarBack,
                             std::vector<Polygon>& front,
                             std::vector<Polygon>& back) {
        constexpr float EPS = 1e-5f;
        enum { COPLANAR = 0, FRONT = 1, BACK = 2, SPANNING = 3 };

        int polygonType = 0;
        const size_t n = poly.verts.size();
        std::vector<int> types(n);
        for (size_t i = 0; i < n; ++i) {
            float t = dot(plane.normal, poly.verts[i]) - plane.w;
            int type = (t < -EPS) ? BACK : (t > EPS) ? FRONT : COPLANAR;
            polygonType |= type;
            types[i] = type;
        }

        switch (polygonType) {
            case COPLANAR:
                (dot(plane.normal, poly.plane.normal) > 0.0f ? coplanarFront
                                                             : coplanarBack)
                    .push_back(poly);
                break;
            case FRONT:
                front.push_back(poly);
                break;
            case BACK:
                back.push_back(poly);
                break;
            default: {
                std::vector<Vector3> f, b;
                for (size_t i = 0; i < n; ++i) {
                    size_t j = (i + 1) % n;
                    int ti = types[i], tj = types[j];
                    const Vector3& vi = poly.verts[i];
                    const Vector3& vj = poly.verts[j];
                    if (ti != BACK) f.push_back(vi);
                    if (ti != FRONT) b.push_back(vi);
                    if ((ti | tj) == SPANNING) {
                        float denom = dot(plane.normal, vj - vi);
                        float tt = (std::abs(denom) > 1e-12f)
                                       ? (plane.w - dot(plane.normal, vi)) / denom
                                       : 0.0f;
                        Vector3 v = vi + (vj - vi) * tt;
                        f.push_back(v);
                        b.push_back(v);
                    }
                }
                if (f.size() >= 3) {
                    Polygon p;
                    p.verts = std::move(f);
                    p.plane = poly.plane;
                    front.push_back(std::move(p));
                }
                if (b.size() >= 3) {
                    Polygon p;
                    p.verts = std::move(b);
                    p.plane = poly.plane;
                    back.push_back(std::move(p));
                }
                break;
            }
        }
    }

    /// @brief Split a triangle by a plane, producing front and back triangles.
    static void splitTriangle(const Plane& plane, const std::array<Vector3, 3>& tri,
                              std::vector<std::array<Vector3, 3>>& front,
                              std::vector<std::array<Vector3, 3>>& back) {
        Polygon poly;
        poly.verts = {tri[0], tri[1], tri[2]};
        poly.plane = makePlane(tri[0], tri[1], tri[2]);
        std::vector<Polygon> cf, cb, fr, bk;
        splitPolygon(plane, poly, cf, cb, fr, bk);
        for (auto& p : cf) fr.push_back(std::move(p));
        for (auto& p : cb) bk.push_back(std::move(p));
        auto emit = [](std::vector<Polygon>& src,
                       std::vector<std::array<Vector3, 3>>& dst) {
            for (auto& p : src)
                for (size_t i = 1; i + 1 < p.verts.size(); ++i)
                    dst.push_back({p.verts[0], p.verts[i], p.verts[i + 1]});
        };
        emit(fr, front);
        emit(bk, back);
    }

    // ── BSP construction ────────────────────────────────────────────────

    /// @brief Build a BSP tree from an indexed triangle mesh.
    static BSPTree buildBSP(const std::vector<Vector3>& verts,
                            const std::vector<unsigned>& indices) {
        BSPTree tree;
        tree.pool.reserve(indices.size());
        std::vector<unsigned> idx;
        idx.reserve(indices.size() / 3);
        for (size_t t = 0; t + 2 < indices.size(); t += 3) {
            Polygon p;
            p.verts = {verts[indices[t]], verts[indices[t + 1]], verts[indices[t + 2]]};
            p.plane = makePlane(p.verts[0], p.verts[1], p.verts[2]);
            if (!p.plane.valid) continue;
            tree.pool.push_back(std::move(p));
            idx.push_back(static_cast<unsigned>(tree.pool.size() - 1));
        }
        tree.root = buildNode(tree.pool, idx);
        return tree;
    }

    /// @brief Classify a triangle against a BSP tree by its centroid.
    static Classification classifyTriangle(const BSPNode* node,
                                           const std::array<Vector3, 3>& tri) {
        Vector3 centroid = (tri[0] + tri[1] + tri[2]) / 3.0f;
        return classifyPoint(node, centroid);
    }

    /// @brief Classify a point as inside/outside/on the solid encoded by a BSP.
    static Classification classifyPoint(const BSPNode* node, const Vector3& p) {
        constexpr float EPS = 1e-5f;
        while (node) {
            float t = dot(node->splitPlane.normal, p) - node->splitPlane.w;
            if (t > EPS) {
                if (node->front) node = node->front;
                else return OUT;
            } else if (t < -EPS) {
                if (node->back) node = node->back;
                else return IN;
            } else {
                if (!node->triangles.empty()) return ON;
                if (node->front) node = node->front;
                else if (node->back) node = node->back;
                else return ON;
            }
        }
        return OUT;
    }

    // ── Boolean operations ──────────────────────────────────────────────

    /// @brief Perform a boolean operation between two meshes.
    static Result booleanOp(const std::vector<Vector3>& vA,
                            const std::vector<unsigned>& iA,
                            const std::vector<Vector3>& vB,
                            const std::vector<unsigned>& iB,
                            Operation op) {
        BSPTree treeA = buildBSP(vA, iA);
        BSPTree treeB = buildBSP(vB, iB);

        std::vector<Polygon> polyA = meshToPolys(vA, iA);
        std::vector<Polygon> polyB = meshToPolys(vB, iB);

        std::vector<Polygon> aIn, aOut, bIn, bOut;
        categorize(treeB.root, polyA, aIn, aOut);
        categorize(treeA.root, polyB, bIn, bOut);

        std::vector<Polygon> out;
        switch (op) {
            case Union:
                append(out, aOut);
                append(out, bOut);
                break;
            case Intersection:
                append(out, aIn);
                append(out, bIn);
                break;
            case Difference:
                append(out, aOut);
                for (auto& p : bIn) {
                    flipPolygon(p);
                    out.push_back(std::move(p));
                }
                break;
        }
        return polysToResult(out);
    }

    static Result unionOp(const std::vector<Vector3>& vA, const std::vector<unsigned>& iA,
                          const std::vector<Vector3>& vB, const std::vector<unsigned>& iB) {
        return booleanOp(vA, iA, vB, iB, Union);
    }
    static Result intersection(const std::vector<Vector3>& vA, const std::vector<unsigned>& iA,
                               const std::vector<Vector3>& vB, const std::vector<unsigned>& iB) {
        return booleanOp(vA, iA, vB, iB, Intersection);
    }
    static Result difference(const std::vector<Vector3>& vA, const std::vector<unsigned>& iA,
                             const std::vector<Vector3>& vB, const std::vector<unsigned>& iB) {
        return booleanOp(vA, iA, vB, iB, Difference);
    }

private:
    static float dot(const Vector3& a, const Vector3& b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }
    static Vector3 cross(const Vector3& a, const Vector3& b) {
        return {a.y * b.z - a.z * b.y,
                a.z * b.x - a.x * b.z,
                a.x * b.y - a.y * b.x};
    }

    static void append(std::vector<Polygon>& dst, std::vector<Polygon>& src) {
        for (auto& p : src) dst.push_back(std::move(p));
    }

    // Recursively build a node from polygon indices into `pool`. Splitting a
    // polygon appends new fragments to `pool`; the picked splitting plane is the
    // supporting plane of the first polygon in the list.
    static BSPNode* buildNode(std::vector<Polygon>& pool, std::vector<unsigned>& polyIdx) {
        if (polyIdx.empty()) return nullptr;
        BSPNode* node = new BSPNode;
        node->splitPlane = pool[polyIdx[0]].plane;

        std::vector<unsigned> frontIdx, backIdx;
        for (unsigned pi : polyIdx) {
            Polygon poly = pool[pi];  // copy: pool may reallocate below
            std::vector<Polygon> cf, cb, fr, bk;
            splitPolygon(node->splitPlane, poly, cf, cb, fr, bk);
            poolAppend(pool, cf, node->triangles);
            poolAppend(pool, cb, node->triangles);
            poolAppend(pool, fr, frontIdx);
            poolAppend(pool, bk, backIdx);
        }
        node->front = buildNode(pool, frontIdx);
        node->back = buildNode(pool, backIdx);
        return node;
    }

    static void poolAppend(std::vector<Polygon>& pool, std::vector<Polygon>& src,
                           std::vector<unsigned>& dst) {
        for (auto& p : src) {
            pool.push_back(std::move(p));
            dst.push_back(static_cast<unsigned>(pool.size() - 1));
        }
    }

    // Push each polygon of `polys` down `node`, collecting fragments that reach
    // an inside (back) leaf or an outside (front) leaf.
    static void categorize(BSPNode* node, std::vector<Polygon>& polys,
                           std::vector<Polygon>& inside,
                           std::vector<Polygon>& outside) {
        if (polys.empty()) return;
        if (!node) {
            for (auto& p : polys) outside.push_back(std::move(p));
            return;
        }
        std::vector<Polygon> frontP, backP, cf, cb;
        for (auto& p : polys)
            splitPolygon(node->splitPlane, p, cf, cb, frontP, backP);
        for (auto& p : cf) frontP.push_back(std::move(p));
        for (auto& p : cb) backP.push_back(std::move(p));

        if (node->front) categorize(node->front, frontP, inside, outside);
        else for (auto& p : frontP) outside.push_back(std::move(p));

        if (node->back) categorize(node->back, backP, inside, outside);
        else for (auto& p : backP) inside.push_back(std::move(p));
    }

    // Fan-triangulate all polygons and weld coincident vertices.
    static Result polysToResult(const std::vector<Polygon>& polys) {
        Result r;
        struct VKey {
            int x, y, z;
            bool operator==(const VKey& o) const {
                return x == o.x && y == o.y && z == o.z;
            }
        };
        struct VKeyHash {
            size_t operator()(const VKey& k) const {
                size_t h = 1469598103934665603ULL;
                auto mix = [&](int v) {
                    h ^= static_cast<size_t>(static_cast<uint32_t>(v));
                    h *= 1099511628211ULL;
                };
                mix(k.x); mix(k.y); mix(k.z);
                return h;
            }
        };
        std::unordered_map<VKey, unsigned, VKeyHash> weld;
        constexpr float INV_EPS = 1.0f / 1e-4f;
        auto addVertex = [&](const Vector3& v) -> unsigned {
            VKey k{static_cast<int>(std::lround(v.x * INV_EPS)),
                   static_cast<int>(std::lround(v.y * INV_EPS)),
                   static_cast<int>(std::lround(v.z * INV_EPS))};
            auto it = weld.find(k);
            if (it != weld.end()) return it->second;
            unsigned id = static_cast<unsigned>(r.verts.size());
            r.verts.push_back(v);
            weld.emplace(k, id);
            return id;
        };

        for (const auto& p : polys) {
            if (p.verts.size() < 3) continue;
            unsigned i0 = addVertex(p.verts[0]);
            for (size_t i = 1; i + 1 < p.verts.size(); ++i) {
                unsigned i1 = addVertex(p.verts[i]);
                unsigned i2 = addVertex(p.verts[i + 1]);
                if (i0 == i1 || i1 == i2 || i0 == i2) continue;
                r.indices.push_back(i0);
                r.indices.push_back(i1);
                r.indices.push_back(i2);
            }
        }
        return r;
    }
};

} // namespace nexus::utility::graphics
