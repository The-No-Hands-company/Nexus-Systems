// The hull used to be checked by COUNTING. Every assertion below the fixed tests was of the
// form "at least 8 vertices, at least 6 faces", and a hull that was none of the things a
// hull is passed all of them.
//
// MEASURED on the eight corners of a cube — the simplest input with a known answer — the old
// implementation returned 12 faces of which 4 were wound outward and 8 inward, 11 of its 25
// directed edges had no reverse (so the surface was not closed), and one face, (1,-1,-1),
// (-1,-1,1), (1,1,1), was a diagonal slicing through the cube's interior rather than a face
// of its boundary. Over 1500 random inputs in five families (points on a ball, points in a
// cube, a 1e-6-thick slab, a tight cluster beside one far point, and integer-grid points)
// NOT ONE hull was closed, and in every family some input point lay OUTSIDE the returned
// hull by more than the model's own size.
//
// Two defects, both structural rather than numerical:
//
//   * The horizon was collected as a set of UNDIRECTED edges, sorted low-index-first. The
//     winding of a new face has to come from the directed boundary edge of the visible
//     region; sorting the endpoints throws that away, so each new face was wound by
//     whichever index happened to be smaller.
//   * Orientation was then patched up by flipping each new normal to face away from
//     `pts[seed[0]]` — a point ON the hull, not inside it. There is no interior point
//     available to a hull that is still growing, and testing against a boundary point gives
//     an arbitrary sign.
//
// It is now an incremental hull built on the exact predicates the kernel already had:
// visibility is the sign of orient3D and carries no epsilon, the horizon keeps its
// direction so a new face inherits the winding of the face it replaces, and the closed-
// surface invariant is checked before anything is returned. The tests assert what a hull IS
// — closed, consistently wound outward, convex, and containing every input point.

#include <gtest/gtest.h>

#include <nexus/geometry/MeshConvexHull.h>
#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/Tolerance.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <random>
#include <utility>
#include <vector>

namespace {

using namespace nexus::geometry;
using nexus::render::Vec3;

// Every directed edge appears exactly once and its reverse exists: a closed, orientable,
// consistently wound triangle surface.
void expectClosedAndConsistentlyWound(const ConvexHull& h, const char* what)
{
    ASSERT_FALSE(h.faces.empty()) << what << ": empty hull";
    std::map<std::pair<uint32_t, uint32_t>, int> directed;
    for (const auto& f : h.faces)
        for (int k = 0; k < 3; ++k)
            ++directed[{f[static_cast<size_t>(k)], f[static_cast<size_t>((k + 1) % 3)]}];

    for (const auto& [e, count] : directed) {
        EXPECT_EQ(count, 1) << what << ": directed edge (" << e.first << "," << e.second
                            << ") used " << count << " times — winding is not consistent";
        EXPECT_TRUE(directed.count({e.second, e.first}))
            << what << ": directed edge (" << e.first << "," << e.second
            << ") has no reverse — the surface is not closed";
    }
}

// Convexity and containment in one sweep: with faces wound counter-clockwise as seen from
// outside, every input point must lie on the inward side of every face plane.
void expectConvexAndContainsAll(const ConvexHull& h, const std::vector<Vec3>& pts,
                                const char* what)
{
    double scale = 1e-12;
    for (const auto& p : pts)
        scale = std::max(scale, std::max({std::fabs((double)p.x), std::fabs((double)p.y),
                                          std::fabs((double)p.z)}));

    double worst = 0.0;
    for (const auto& f : h.faces) {
        const Vec3 a = h.vertices[f[0]], b = h.vertices[f[1]], c = h.vertices[f[2]];
        const double nx = (double)(b.y - a.y) * (c.z - a.z) - (double)(b.z - a.z) * (c.y - a.y);
        const double ny = (double)(b.z - a.z) * (c.x - a.x) - (double)(b.x - a.x) * (c.z - a.z);
        const double nz = (double)(b.x - a.x) * (c.y - a.y) - (double)(b.y - a.y) * (c.x - a.x);
        const double len = std::sqrt(nx * nx + ny * ny + nz * nz);
        if (len <= 0.0) { ADD_FAILURE() << what << ": degenerate hull face"; continue; }
        for (const auto& p : pts) {
            const double d = (nx * ((double)p.x - a.x) + ny * ((double)p.y - a.y) +
                              nz * ((double)p.z - a.z)) / len;
            worst = std::max(worst, d);
        }
    }
    EXPECT_LT(worst / scale, 1e-5)
        << what << ": an input point lies OUTSIDE the hull by " << worst << " (" << worst / scale
        << " relative) — the hull is not convex or does not contain its input";
}

TEST(MeshConvexHull, TetrahedronFromFourPoints)
{
    std::vector<Vec3> points = {
        {0.f, 0.f, 0.f},
        {1.f, 0.f, 0.f},
        {0.f, 1.f, 0.f},
        {0.f, 0.f, 1.f},
    };
    auto hull = MeshConvexHull::build(points);
    EXPECT_EQ(hull.vertices.size(), 4u);
    EXPECT_GE(hull.faces.size(), 4u);
}

// The case that exposed it. A cube's hull is the cube: 8 vertices, 12 triangles, and not one
// face through the interior.
TEST(MeshConvexHull, CubeFromEightPoints)
{
    std::vector<Vec3> points = {
        {-1.f, -1.f, -1.f},
        { 1.f, -1.f, -1.f},
        { 1.f,  1.f, -1.f},
        {-1.f,  1.f, -1.f},
        {-1.f, -1.f,  1.f},
        { 1.f, -1.f,  1.f},
        { 1.f,  1.f,  1.f},
        {-1.f,  1.f,  1.f},
    };
    auto hull = MeshConvexHull::build(points);
    EXPECT_EQ(hull.vertices.size(), 8u);
    EXPECT_EQ(hull.faces.size(), 12u) << "a cube's hull is 6 quads = 12 triangles";
    expectClosedAndConsistentlyWound(hull, "cube");
    expectConvexAndContainsAll(hull, points, "cube");

    // every face lies in one of the six cube planes — no interior diagonals
    for (const auto& f : hull.faces) {
        const Vec3 a = hull.vertices[f[0]], b = hull.vertices[f[1]], c = hull.vertices[f[2]];
        const bool planar =
            (a.x == b.x && b.x == c.x) || (a.y == b.y && b.y == c.y) || (a.z == b.z && b.z == c.z);
        EXPECT_TRUE(planar) << "hull face (" << a.x << "," << a.y << "," << a.z << ") ("
                            << b.x << "," << b.y << "," << b.z << ") (" << c.x << "," << c.y
                            << "," << c.z << ") is not on the cube's boundary";
    }
}

// Interior points must not become hull vertices, and must not perturb the answer.
TEST(MeshConvexHull, InteriorPointsAreExcluded)
{
    std::vector<Vec3> points = {
        {-1.f, -1.f, -1.f}, { 1.f, -1.f, -1.f}, { 1.f,  1.f, -1.f}, {-1.f,  1.f, -1.f},
        {-1.f, -1.f,  1.f}, { 1.f, -1.f,  1.f}, { 1.f,  1.f,  1.f}, {-1.f,  1.f,  1.f},
        {0.f, 0.f, 0.f}, {0.5f, -0.25f, 0.1f}, {-0.9f, 0.9f, 0.0f},
    };
    auto hull = MeshConvexHull::build(points);
    EXPECT_EQ(hull.vertices.size(), 8u) << "an interior point was kept as a hull vertex";
    EXPECT_EQ(hull.faces.size(), 12u);
    expectClosedAndConsistentlyWound(hull, "cube+interior");
    expectConvexAndContainsAll(hull, points, "cube+interior");
}

// Points ON a face and ON an edge of the hull are not outside it, so they may not push the
// hull outwards — the classic exactly-coplanar case a visibility epsilon gets wrong.
TEST(MeshConvexHull, CoplanarAndCollinearBoundaryPointsDoNotInflateTheHull)
{
    std::vector<Vec3> points = {
        {-1.f, -1.f, -1.f}, { 1.f, -1.f, -1.f}, { 1.f,  1.f, -1.f}, {-1.f,  1.f, -1.f},
        {-1.f, -1.f,  1.f}, { 1.f, -1.f,  1.f}, { 1.f,  1.f,  1.f}, {-1.f,  1.f,  1.f},
        {0.f, 0.f, -1.f},   // centre of a face
        {0.f, -1.f, -1.f},  // midpoint of an edge
        {0.5f, 1.f, 0.25f}, // interior of another face
    };
    auto hull = MeshConvexHull::build(points);
    expectClosedAndConsistentlyWound(hull, "cube+boundary");
    expectConvexAndContainsAll(hull, points, "cube+boundary");
    for (const auto& v : hull.vertices) {
        EXPECT_EQ(std::fabs(v.x), 1.f);
        EXPECT_EQ(std::fabs(v.y), 1.f);
        EXPECT_EQ(std::fabs(v.z), 1.f);
    }
}

// The adversarial families. Each one produced zero closed hulls before the rewrite.
TEST(MeshConvexHull, RandomInputFamiliesAlwaysProduceAClosedConvexHull)
{
    std::mt19937 rng(12345u);
    std::uniform_real_distribution<float> u(-1.f, 1.f);
    std::uniform_int_distribution<int> gi(-3, 3);

    for (int it = 0; it < 40; ++it) {
        {   // points on a sphere — every point is a hull vertex
            std::vector<Vec3> p;
            for (int i = 0; i < 40; ++i) {
                Vec3 v{u(rng), u(rng), u(rng)};
                const float l = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
                if (l > 1e-6f) { v.x /= l; v.y /= l; v.z /= l; p.push_back(v); }
            }
            auto h = MeshConvexHull::build(p);
            expectClosedAndConsistentlyWound(h, "ball");
            expectConvexAndContainsAll(h, p, "ball");
        }
        {   // solid cube of points — most are interior
            std::vector<Vec3> p;
            for (int i = 0; i < 60; ++i) p.push_back({u(rng), u(rng), u(rng)});
            auto h = MeshConvexHull::build(p);
            expectClosedAndConsistentlyWound(h, "cube-cloud");
            expectConvexAndContainsAll(h, p, "cube-cloud");
        }
        {   // a 1e-6-thick slab: nearly, but not exactly, coplanar
            std::vector<Vec3> p;
            for (int i = 0; i < 40; ++i) p.push_back({u(rng), u(rng), u(rng) * 1e-6f});
            auto h = MeshConvexHull::build(p);
            if (!h.faces.empty()) {
                expectClosedAndConsistentlyWound(h, "slab");
                expectConvexAndContainsAll(h, p, "slab");
            }
        }
        {   // a tight cluster and one far point: an extreme aspect ratio
            std::vector<Vec3> p;
            for (int i = 0; i < 30; ++i) p.push_back({u(rng) * 1e-4f, u(rng) * 1e-4f, u(rng) * 1e-4f});
            p.push_back({1.f, 0.f, 0.f});
            auto h = MeshConvexHull::build(p);
            expectClosedAndConsistentlyWound(h, "cluster");
            expectConvexAndContainsAll(h, p, "cluster");
        }
        {   // integer grid: exactly-coplanar and exactly-collinear sets are everywhere, which
            // is where a relative epsilon has no right answer and an exact predicate does
            std::vector<Vec3> p;
            for (int i = 0; i < 40; ++i)
                p.push_back({(float)gi(rng), (float)gi(rng), (float)gi(rng)});
            auto h = MeshConvexHull::build(p);
            expectClosedAndConsistentlyWound(h, "grid");
            expectConvexAndContainsAll(h, p, "grid");
        }
    }
}

// Determinism: the same input must give the same hull, vertex for vertex and face for face.
TEST(MeshConvexHull, IsDeterministic)
{
    std::mt19937 rng(777u);
    std::uniform_real_distribution<float> u(-1.f, 1.f);
    std::vector<Vec3> p;
    for (int i = 0; i < 50; ++i) p.push_back({u(rng), u(rng), u(rng)});

    const auto a = MeshConvexHull::build(p);
    const auto b = MeshConvexHull::build(p);
    ASSERT_EQ(a.vertices.size(), b.vertices.size());
    ASSERT_EQ(a.faces.size(), b.faces.size());
    for (size_t i = 0; i < a.vertices.size(); ++i) {
        EXPECT_EQ(a.vertices[i].x, b.vertices[i].x);
        EXPECT_EQ(a.vertices[i].y, b.vertices[i].y);
        EXPECT_EQ(a.vertices[i].z, b.vertices[i].z);
    }
    for (size_t i = 0; i < a.faces.size(); ++i) EXPECT_EQ(a.faces[i], b.faces[i]);
}

// Duplicated input is not degenerate input.
TEST(MeshConvexHull, RepeatedPointsAreTolerated)
{
    std::vector<Vec3> points;
    for (int rep = 0; rep < 3; ++rep) {
        points.push_back({-1.f, -1.f, -1.f}); points.push_back({ 1.f, -1.f, -1.f});
        points.push_back({ 1.f,  1.f, -1.f}); points.push_back({-1.f,  1.f, -1.f});
        points.push_back({-1.f, -1.f,  1.f}); points.push_back({ 1.f, -1.f,  1.f});
        points.push_back({ 1.f,  1.f,  1.f}); points.push_back({-1.f,  1.f,  1.f});
    }
    auto hull = MeshConvexHull::build(points);
    EXPECT_EQ(hull.vertices.size(), 8u);
    EXPECT_EQ(hull.faces.size(), 12u);
    expectClosedAndConsistentlyWound(hull, "repeated");
    expectConvexAndContainsAll(hull, points, "repeated");
}

// A non-finite coordinate may not corrupt the hull of the finite points around it.
TEST(MeshConvexHull, NonFiniteInputIsRejectedNotPropagated)
{
    const float inf = std::numeric_limits<float>::infinity();
    const float nan = std::numeric_limits<float>::quiet_NaN();
    std::vector<Vec3> points = {
        {-1.f, -1.f, -1.f}, { 1.f, -1.f, -1.f}, { 1.f,  1.f, -1.f}, {-1.f,  1.f, -1.f},
        {-1.f, -1.f,  1.f}, { 1.f, -1.f,  1.f}, { 1.f,  1.f,  1.f}, {-1.f,  1.f,  1.f},
        {inf, 0.f, 0.f}, {0.f, nan, 0.f}, {-inf, nan, inf},
    };
    auto hull = MeshConvexHull::build(points);
    EXPECT_EQ(hull.vertices.size(), 8u);
    EXPECT_EQ(hull.faces.size(), 12u);
    expectClosedAndConsistentlyWound(hull, "non-finite");
    // geometry::isFinite inspects the exponent bits; std::isfinite is the one the project
    // does not trust for this (see the -ffast-math note in CLAUDE.md).
    for (const auto& v : hull.vertices) {
        EXPECT_TRUE(isFinite(v.x) && isFinite(v.y) && isFinite(v.z));
    }
}

TEST(MeshConvexHull, FromMeshConveniencePasses)
{
    auto box = primitives::makeBox(2.f, 2.f, 2.f);
    ASSERT_TRUE(box.isValid());
    auto hull = MeshConvexHull::fromMesh(box);
    EXPECT_EQ(hull.vertices.size(), 8u);
    EXPECT_EQ(hull.faces.size(), 12u);
    expectClosedAndConsistentlyWound(hull, "fromMesh(box)");
    expectConvexAndContainsAll(hull, box.attributes().positions(), "fromMesh(box)");
}

TEST(MeshConvexHull, TooFewPointsFails)
{
    std::vector<Vec3> points = {
        {0.f, 0.f, 0.f},
        {1.f, 0.f, 0.f},
        {0.f, 1.f, 0.f},
    };
    auto hull = MeshConvexHull::build(points);
    EXPECT_TRUE(hull.vertices.empty());
    EXPECT_TRUE(hull.faces.empty());
}

TEST(MeshConvexHull, CoplanarPointsFails)
{
    std::vector<Vec3> points = {
        {0.f, 0.f, 0.f},
        {1.f, 0.f, 0.f},
        {0.f, 1.f, 0.f},
        {1.f, 1.f, 0.f},
    };
    auto hull = MeshConvexHull::build(points);
    EXPECT_TRUE(hull.faces.empty());
}

TEST(MeshConvexHull, SinglePointFails)
{
    std::vector<Vec3> points = {
        {0.f, 0.f, 0.f},
    };
    auto hull = MeshConvexHull::build(points);
    EXPECT_TRUE(hull.vertices.empty());
    EXPECT_TRUE(hull.faces.empty());
}

TEST(MeshConvexHull, CollinearPointsFail)
{
    std::vector<Vec3> points = {
        {0.f, 0.f, 0.f},
        {1.f, 0.f, 0.f},
        {2.f, 0.f, 0.f},
    };
    auto hull = MeshConvexHull::build(points);
    EXPECT_TRUE(hull.faces.empty());
}

} // namespace
