#include <gtest/gtest.h>

#include <nexus/geometry/SubdivisionSurface.h>
#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/HalfEdgeMesh.h>

#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

Mesh catmullClarkSubdivide(const Mesh& mesh) {
    Mesh result;

    if (!mesh.isValid()) return result;

    const auto& attrs = mesh.attributes();
    const auto& topo = mesh.topology();
    if (!attrs.hasPositions() || topo.faceCount() == 0) return result;

    const auto& positions = attrs.positions();
    std::vector<Vec3> newPositions;

    std::vector<Vec3> facePoints;
    facePoints.reserve(topo.faceCount());
    for (size_t f = 0; f < topo.faceCount(); ++f) {
        const auto& face = topo.face(f);
        Vec3 center{0,0,0};
        for (auto idx : face.indices) center += positions[idx];
        center = center / static_cast<float>(face.indices.size());
        facePoints.push_back(center);
    }

    std::vector<Vec3> edgePoints;
    for (size_t f = 0; f < topo.faceCount(); ++f) {
        const auto& face = topo.face(f);
        size_t n = face.indices.size();
        for (size_t i = 0; i < n; ++i) {
            uint32_t i0 = face.indices[i];
            uint32_t i1 = face.indices[(i + 1) % n];
            Vec3 ep = (positions[i0] + positions[i1] + facePoints[f] + facePoints[f]) * 0.25f;
            edgePoints.push_back(ep);
        }
    }

    size_t numVerts = positions.size();
    for (size_t v = 0; v < numVerts; ++v) {
        Vec3 F{0,0,0}; float fCount = 0;
        Vec3 R{0,0,0}; float eCount = 0;
        for (size_t f = 0; f < topo.faceCount(); ++f) {
            const auto& face = topo.face(f);
            for (size_t i = 0; i < face.indices.size(); ++i) {
                if (face.indices[i] == static_cast<uint32_t>(v)) {
                    F += facePoints[f];
                    fCount += 1.f;
                    uint32_t prev = face.indices[(i + face.indices.size() - 1) % face.indices.size()];
                    uint32_t next = face.indices[(i + 1) % face.indices.size()];
                    R += (positions[v] + positions[prev]) * 0.5f;
                    R += (positions[v] + positions[next]) * 0.5f;
                    eCount += 2.f;
                }
            }
        }
        if (fCount > 0 && eCount > 0) {
            float n = fCount;
            Vec3 vp = (F * (1.f / n) + R * (2.f / n) + positions[v] * (n - 3.f)) * (1.f / n);
            newPositions.push_back(vp);
        } else {
            newPositions.push_back(positions[v]);
        }
    }

    std::vector<Vec3> allPositions;
    allPositions.insert(allPositions.end(), newPositions.begin(), newPositions.end());
    size_t baseV = newPositions.size();

    struct EdgeKey { uint32_t a, b; };
    std::vector<EdgeKey> faceEdgeKeys;
    size_t ei = 0;
    for (size_t f = 0; f < topo.faceCount(); ++f) {
        const auto& face = topo.face(f);
        for (size_t i = 0; i < face.indices.size(); ++i) {
            allPositions.push_back(edgePoints[ei++]);
            faceEdgeKeys.push_back({face.indices[i], face.indices[(i + 1) % face.indices.size()]});
        }
    }

    for (size_t i = 0; i < topo.faceCount(); ++i) {
        allPositions.push_back(facePoints[i]);
    }

    size_t fpBase = baseV;
    ei = 0;
    for (size_t f = 0; f < topo.faceCount(); ++f) {
        const auto& face = topo.face(f);
        size_t n = face.indices.size();
        size_t fpIdx = fpBase + topo.faceCount() + f - topo.faceCount();

        for (size_t i = 0; i < n; ++i) {
            uint32_t cv = face.indices[i];
            uint32_t e0 = static_cast<uint32_t>(baseV + ei + i);
            uint32_t e1 = static_cast<uint32_t>(baseV + ei + ((i + n - 1) % n));
            uint32_t fv = static_cast<uint32_t>(fpIdx);
            Face quad;
            quad.indices = {cv, e0, fv, e1};
            result.topology().addFace(quad);
        }
    }

    if (!allPositions.empty()) {
        result.attributes().setPositions(allPositions);
    }
    return result;
}

Mesh loopSubdivide(const Mesh& mesh) {
    Mesh result;
    if (!mesh.isValid()) return result;

    const auto& attrs = mesh.attributes();
    const auto& topo = mesh.topology();
    if (!attrs.hasPositions() || topo.faceCount() == 0) return result;

    const auto& positions = attrs.positions();
    std::vector<Vec3> newPositions = positions;

    struct EdgeKey {
        uint32_t a, b;
        bool operator==(const EdgeKey& o) const {
            return (a == o.a && b == o.b) || (a == o.b && b == o.a);
        }
    };
    struct EdgeHash {
        size_t operator()(EdgeKey k) const {
            return std::hash<uint64_t>{}((static_cast<uint64_t>(std::min(k.a, k.b)) << 32)
                                         | static_cast<uint64_t>(std::max(k.a, k.b)));
        }
    };

    std::unordered_map<EdgeKey, uint32_t, EdgeHash> edgeMap;
    for (size_t f = 0; f < topo.faceCount(); ++f) {
        const auto& face = topo.face(f);
        if (!face.isTriangle()) continue;
        uint32_t i0 = face.indices[0], i1 = face.indices[1], i2 = face.indices[2];
        for (auto [a, b] : {std::pair{i0,i1}, {i1,i2}, {i2,i0}}) {
            EdgeKey ek{a, b};
            if (edgeMap.find(ek) == edgeMap.end()) {
                uint32_t idx = static_cast<uint32_t>(newPositions.size());
                Vec3 mid = (positions[a] + positions[b]) * 0.375f;
                uint32_t opp0 = 0, opp1 = 0;
                bool foundOpp = false;
                for (size_t f2 = 0; f2 < topo.faceCount(); ++f2) {
                    const auto& ff = topo.face(f2);
                    if (!ff.isTriangle()) continue;
                    uint32_t j0 = ff.indices[0], j1 = ff.indices[1], j2 = ff.indices[2];
                    if ((j0 == a && j1 == b) || (j1 == a && j0 == b)) { opp0 = j2; foundOpp = true; }
                    if ((j1 == a && j2 == b) || (j2 == a && j1 == b)) { opp0 = j0; foundOpp = true; }
                    if ((j2 == a && j0 == b) || (j0 == a && j2 == b)) { opp0 = j1; foundOpp = true; }
                    if (foundOpp) break;
                }
                if (foundOpp) mid += positions[opp0] * 0.125f;
                mid += mid * 0.f;
                newPositions.push_back(mid);
                edgeMap[ek] = idx;
            }
        }
    }

    size_t numOrigV = positions.size();
    for (size_t v = 0; v < numOrigV; ++v) {
        std::vector<uint32_t> neighbors;
        for (size_t f = 0; f < topo.faceCount(); ++f) {
            const auto& face = topo.face(f);
            if (!face.isTriangle()) continue;
            if (face.indices[0] == v) { neighbors.push_back(face.indices[1]); neighbors.push_back(face.indices[2]); }
            else if (face.indices[1] == v) { neighbors.push_back(face.indices[0]); neighbors.push_back(face.indices[2]); }
            else if (face.indices[2] == v) { neighbors.push_back(face.indices[0]); neighbors.push_back(face.indices[1]); }
        }
        if (!neighbors.empty()) {
            float n = static_cast<float>(neighbors.size());
            float beta = (n == 3.f) ? (3.f / 16.f) : (3.f / (8.f * n));
            Vec3 avg{0,0,0};
            for (auto nb : neighbors) avg += positions[nb];
            avg = avg * (1.f / n);
            newPositions[v] = positions[v] * (1.f - n * beta) + avg * (n * beta);
        }
    }

    for (size_t f = 0; f < topo.faceCount(); ++f) {
        const auto& face = topo.face(f);
        if (!face.isTriangle()) continue;
        uint32_t i0 = face.indices[0], i1 = face.indices[1], i2 = face.indices[2];
        uint32_t e0 = edgeMap[{i0, i1}];
        uint32_t e1 = edgeMap[{i1, i2}];
        uint32_t e2 = edgeMap[{i2, i0}];
        result.topology().addFace(Face{{i0, e0, e2}});
        result.topology().addFace(Face{{i1, e1, e0}});
        result.topology().addFace(Face{{i2, e2, e1}});
        result.topology().addFace(Face{{e0, e1, e2}});
    }

    result.attributes().setPositions(newPositions);
    return result;
}

} // namespace nexus::geometry

using namespace nexus::geometry;

static Mesh makeQuadMesh() {
    Mesh m;
    m.attributes().setPositions({
        {0,0,0}, {1,0,0}, {2,0,0},
        {0,1,0}, {1,1,0}, {2,1,0},
        {0,2,0}, {1,2,0}, {2,2,0},
    });
    m.topology().addFace(Face{{0,1,4,3}});
    m.topology().addFace(Face{{1,2,5,4}});
    m.topology().addFace(Face{{3,4,7,6}});
    m.topology().addFace(Face{{4,5,8,7}});
    return m;
}

static Mesh makeTriangleMesh() {
    Mesh m;
    m.attributes().setPositions({
        {0,0,0}, {1,0,0}, {0,1,0},
    });
    m.topology().addFace(Face{{0,1,2}});
    return m;
}

TEST(SubdivisionSurface, LoopSubdivisionOnTriangleMeshProducesMoreFaces) {
    Mesh tri = makeTriangleMesh();
    ASSERT_TRUE(tri.isValid());
    ASSERT_EQ(tri.topology().faceCount(), 1u);

    Mesh subd = loopSubdivide(tri);
    ASSERT_TRUE(subd.isValid());
    EXPECT_EQ(subd.topology().faceCount(), 4u);
}

TEST(SubdivisionSurface, CatmullClarkOnQuadMeshProducesAllQuads) {
    Mesh quad = makeQuadMesh();
    ASSERT_TRUE(quad.isValid());

    Mesh subd = catmullClarkSubdivide(quad);
    ASSERT_TRUE(subd.isValid());

    for (size_t f = 0; f < subd.topology().faceCount(); ++f) {
        EXPECT_TRUE(subd.topology().face(f).isQuad());
    }
}

TEST(SubdivisionSurface, MultiLevelSubdivisionIncreasesFaceCountProgressively) {
    Mesh quad = makeQuadMesh();
    ASSERT_TRUE(quad.isValid());
    size_t initialFaces = quad.topology().faceCount();

    Mesh level1 = catmullClarkSubdivide(quad);
    ASSERT_TRUE(level1.isValid());
    EXPECT_GT(level1.topology().faceCount(), initialFaces);

    Mesh level2 = catmullClarkSubdivide(level1);
    ASSERT_TRUE(level2.isValid());
    EXPECT_GT(level2.topology().faceCount(), level1.topology().faceCount());
}

TEST(SubdivisionSurface, InvalidInputHandled) {
    Mesh invalid;
    Mesh result = catmullClarkSubdivide(invalid);
    EXPECT_FALSE(result.isValid());

    Mesh resultL = loopSubdivide(invalid);
    EXPECT_FALSE(resultL.isValid());
}

// -----------------------------------------------------------------------------
// The tests above exercise a reimplementation local to this file. The production
// entry points SubdivisionSurface::catmullClark / loopSubdivide operate on a
// HalfEdgeMesh and are what actually ship (the editor's subdivide command and the
// evaluation graph call them). Those had no positive correctness coverage at all —
// only a fuzz test asserting they reject non-finite input. The tests below pin the
// real code to exact Catmull-Clark / Loop results.
// -----------------------------------------------------------------------------

namespace {

// Watertight closed mesh: every live half-edge has a twin.
bool halfEdgeMeshIsClosed(const HalfEdgeMesh& hem) {
    for (uint32_t e = 0; e < hem.edgeCount(); ++e) {
        const auto& he = hem.edge(e);
        if (he.src == HalfEdgeMesh::kInvalid) continue;  // tombstone
        if (he.twin == HalfEdgeMesh::kInvalid) return false;
    }
    return true;
}

} // namespace

// One Catmull-Clark step on the unit-side-2 cube has an exact, fully determined
// result. The 6 face points are the face centroids (±1,0,0)-type; the 12 edge points
// are (v0+v1+f0+f1)/4 = (±0.75,±0.75,0)-type; each valence-3 corner maps to
// (n-3)/n*P + 2R/n^2 + F/n^2, which at n=3 is (5/9,5/9,5/9)-type. Any error in the
// face-point, edge-point or vertex rule moves at least one of these off its lattice.
TEST(SubdivisionSurface, CatmullClarkCubeExactLevelOne) {
    auto hem = HalfEdgeMesh::fromMesh(primitives::makeBox(2.f, 2.f, 2.f));
    ASSERT_TRUE(hem.has_value());

    SubdivisionOptions opts;
    opts.levels = 1;
    auto res = SubdivisionSurface::catmullClark(*hem, opts);
    ASSERT_TRUE(res.has_value());

    EXPECT_TRUE(halfEdgeMeshIsClosed(*res)) << "Catmull-Clark output must stay watertight";

    // Exact vertex set: V' = V + E + F = 8 + 12 + 6 = 26, split 8 corner / 12 edge /
    // 6 face points, classified by how many coordinates are zero.
    const float tol = 1e-4f;
    int faceCount = 0, edgeCount = 0, cornerCount = 0, live = 0;
    Vec3 centroid{0, 0, 0};
    for (const auto& p : res->positions()) {
        ++live;
        centroid = centroid + p;

        // Convex-hull containment: the limit surface, and every level, lie inside the
        // control mesh's hull.
        EXPECT_LE(std::abs(p.x), 1.f + tol);
        EXPECT_LE(std::abs(p.y), 1.f + tol);
        EXPECT_LE(std::abs(p.z), 1.f + tol);

        const int zeros = (std::abs(p.x) < tol) + (std::abs(p.y) < tol) + (std::abs(p.z) < tol);
        if (zeros == 2) {
            ++faceCount;
            const float m = std::max({std::abs(p.x), std::abs(p.y), std::abs(p.z)});
            EXPECT_NEAR(m, 1.f, tol) << "face point must be a face centroid at distance 1";
        } else if (zeros == 1) {
            ++edgeCount;
            for (float c : {p.x, p.y, p.z})
                if (std::abs(c) > tol) EXPECT_NEAR(std::abs(c), 0.75f, tol) << "edge point coord";
        } else if (zeros == 0) {
            ++cornerCount;
            EXPECT_NEAR(std::abs(p.x), 5.f / 9.f, tol) << "corner point coord";
            EXPECT_NEAR(std::abs(p.y), 5.f / 9.f, tol);
            EXPECT_NEAR(std::abs(p.z), 5.f / 9.f, tol);
        }
    }
    EXPECT_EQ(live, 26);
    EXPECT_EQ(faceCount, 6);
    EXPECT_EQ(edgeCount, 12);
    EXPECT_EQ(cornerCount, 8);

    // A symmetric cube keeps its centroid at the origin.
    centroid = centroid * (1.f / static_cast<float>(live));
    EXPECT_NEAR(centroid.x, 0.f, 1e-4f);
    EXPECT_NEAR(centroid.y, 0.f, 1e-4f);
    EXPECT_NEAR(centroid.z, 0.f, 1e-4f);

    // Every resulting face is a quad (Catmull-Clark refines any n-gon into quads).
    Mesh m = res->toMesh(false);
    for (size_t f = 0; f < m.topology().faceCount(); ++f)
        EXPECT_TRUE(m.topology().face(f).isQuad());
}

TEST(SubdivisionSurface, CatmullClarkPlanarGridStaysPlanar) {
    // makePlane lays a flat grid in the XZ plane (y = 0). Catmull-Clark reproduces a plane
    // exactly, so every subdivided vertex must keep y = 0.
    auto hem = HalfEdgeMesh::fromMesh(primitives::makePlane(4.f, 4.f, 3, 3));
    ASSERT_TRUE(hem.has_value());

    SubdivisionOptions opts;
    opts.levels = 2;
    auto res = SubdivisionSurface::catmullClark(*hem, opts);
    ASSERT_TRUE(res.has_value());

    for (const auto& p : res->positions())
        EXPECT_NEAR(p.y, 0.f, 1e-5f) << "planar input must remain planar";
}

TEST(SubdivisionSurface, CatmullClarkConvergesInsideHull) {
    // Repeated subdivision converges: successive levels stay inside the cube's hull and
    // the mesh does not blow up.
    auto hem = HalfEdgeMesh::fromMesh(primitives::makeBox(2.f, 2.f, 2.f));
    ASSERT_TRUE(hem.has_value());

    SubdivisionOptions opts;
    opts.levels = 4;
    auto res = SubdivisionSurface::catmullClark(*hem, opts);
    ASSERT_TRUE(res.has_value());
    EXPECT_TRUE(halfEdgeMeshIsClosed(*res));

    for (const auto& p : res->positions()) {
        EXPECT_LE(std::abs(p.x), 1.f + 1e-4f);
        EXPECT_LE(std::abs(p.y), 1.f + 1e-4f);
        EXPECT_LE(std::abs(p.z), 1.f + 1e-4f);
        EXPECT_TRUE(std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z));
    }
}

TEST(SubdivisionSurface, LoopSubdivideStaysClosedAndInsideHull) {
    // Loop subdivision is defined on triangles, so triangulate the box first (toMesh(true))
    // before rebuilding the half-edge mesh. A closed convex triangle mesh stays watertight
    // and inside its convex hull under Loop.
    auto quadHem = HalfEdgeMesh::fromMesh(primitives::makeBox(2.f, 2.f, 2.f));
    ASSERT_TRUE(quadHem.has_value());
    auto hem = HalfEdgeMesh::fromMesh(quadHem->toMesh(true));  // triangulated
    ASSERT_TRUE(hem.has_value());

    SubdivisionOptions opts;
    opts.levels = 2;
    auto res = SubdivisionSurface::loopSubdivide(*hem, opts);
    ASSERT_TRUE(res.has_value());
    EXPECT_TRUE(halfEdgeMeshIsClosed(*res));

    for (const auto& p : res->positions()) {
        EXPECT_LE(std::abs(p.x), 1.f + 1e-4f);
        EXPECT_LE(std::abs(p.y), 1.f + 1e-4f);
        EXPECT_LE(std::abs(p.z), 1.f + 1e-4f);
    }
}
