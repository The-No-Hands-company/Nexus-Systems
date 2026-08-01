#include <gtest/gtest.h>

#include <nexus/geometry/TetDelaunay3D.h>
#include <nexus/geometry/MeshConvexHull.h>
#include <nexus/geometry/RobustPredicates.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <random>
#include <utility>
#include <vector>

// TetDelaunay3D (incremental Bowyer-Watson 3D Delaunay tetrahedralization) shipped with no
// test coverage at all. These tests pin it to the defining Delaunay properties: valid
// non-degenerate tets, every input vertex used, the empty-circumsphere property (no vertex
// strictly inside any tet's circumsphere), and an exact tiling of the input's convex hull.
//
// CORRECTION: this comment used to say "it turns out to be correct". Its circumsphere
// property was, and is; its HULL COVERAGE was not, and the fixed-shape fixtures below could
// not see it. On 25 uniformly random points in a cube the tetrahedra covered less than the
// convex hull in 50 of 60 point sets, by up to 1.8% of the volume — the super-tetrahedron
// sizing problem already documented and solved for the 2D triangulators in
// DelaunaySuperTriangle.h: a sliver tet's circumsphere swallows the super-tetrahedron's
// vertices, so that tet is never emitted, and stripping the super-tetrahedron at the end
// deletes real volume. The fixtures here are well conditioned enough that the historical
// fixed scale happened to suffice for them. The randomized coverage test at the bottom is
// the one that fails on the old code.

namespace {

using namespace nexus::geometry;
using nexus::render::Vec3;

double tetVolume(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d) {
    const double ux = b.x - a.x, uy = b.y - a.y, uz = b.z - a.z;
    const double vx = c.x - a.x, vy = c.y - a.y, vz = c.z - a.z;
    const double wx = d.x - a.x, wy = d.y - a.y, wz = d.z - a.z;
    const double det = ux * (vy * wz - vz * wy)
                     - uy * (vx * wz - vz * wx)
                     + uz * (vx * wy - vy * wx);
    return std::abs(det) / 6.0;
}

// Circumsphere from four points via the normal-equation solve, in double.
bool circumsphere(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d,
                  double& cx, double& cy, double& cz, double& r2) {
    double A[3][3] = {
        {2.0 * (b.x - a.x), 2.0 * (b.y - a.y), 2.0 * (b.z - a.z)},
        {2.0 * (c.x - a.x), 2.0 * (c.y - a.y), 2.0 * (c.z - a.z)},
        {2.0 * (d.x - a.x), 2.0 * (d.y - a.y), 2.0 * (d.z - a.z)},
    };
    const double a2 = double(a.x) * a.x + double(a.y) * a.y + double(a.z) * a.z;
    double B[3] = {
        double(b.x) * b.x + double(b.y) * b.y + double(b.z) * b.z - a2,
        double(c.x) * c.x + double(c.y) * c.y + double(c.z) * c.z - a2,
        double(d.x) * d.x + double(d.y) * d.y + double(d.z) * d.z - a2,
    };
    const double det = A[0][0] * (A[1][1] * A[2][2] - A[1][2] * A[2][1])
                     - A[0][1] * (A[1][0] * A[2][2] - A[1][2] * A[2][0])
                     + A[0][2] * (A[1][0] * A[2][1] - A[1][1] * A[2][0]);
    if (std::abs(det) < 1e-30) return false;
    const double inv = 1.0 / det;
    auto cramer = [&](int col) {
        double M[3][3] = {{A[0][0], A[0][1], A[0][2]},
                          {A[1][0], A[1][1], A[1][2]},
                          {A[2][0], A[2][1], A[2][2]}};
        M[0][col] = B[0]; M[1][col] = B[1]; M[2][col] = B[2];
        return (M[0][0] * (M[1][1] * M[2][2] - M[1][2] * M[2][1])
              - M[0][1] * (M[1][0] * M[2][2] - M[1][2] * M[2][0])
              + M[0][2] * (M[1][0] * M[2][1] - M[1][1] * M[2][0])) * inv;
    };
    cx = cramer(0); cy = cramer(1); cz = cramer(2);
    const double ex = cx - a.x, ey = cy - a.y, ez = cz - a.z;
    r2 = ex * ex + ey * ey + ez * ez;
    return true;
}

// Assert the standard Delaunay health invariants on a result. Returns total tet volume.
double expectHealthy(const TetMesh& m, const std::vector<Vec3>& pts) {
    const uint32_t n = static_cast<uint32_t>(pts.size());
    double total = 0.0;
    std::vector<bool> used(n, false);
    for (const auto& t : m.tetrahedra) {
        bool inRange = true;
        for (int k = 0; k < 4; ++k) {
            if (t[k] >= n) { inRange = false; }
        }
        EXPECT_TRUE(inRange) << "tet references a super-vertex or out-of-range index";
        if (!inRange) continue;
        for (int k = 0; k < 4; ++k) used[t[k]] = true;
        const double v = tetVolume(pts[t[0]], pts[t[1]], pts[t[2]], pts[t[3]]);
        EXPECT_GT(v, 1e-11) << "degenerate (zero-volume) tet emitted";
        total += v;
    }
    for (uint32_t i = 0; i < n; ++i) EXPECT_TRUE(used[i]) << "input vertex " << i << " unused";

    // Empty-circumsphere property: no OTHER input vertex strictly inside any tet's sphere.
    for (const auto& t : m.tetrahedra) {
        double cx, cy, cz, r2;
        if (!circumsphere(pts[t[0]], pts[t[1]], pts[t[2]], pts[t[3]], cx, cy, cz, r2)) continue;
        for (uint32_t vi = 0; vi < n; ++vi) {
            if (vi == t[0] || vi == t[1] || vi == t[2] || vi == t[3]) continue;
            const double dx = pts[vi].x - cx, dy = pts[vi].y - cy, dz = pts[vi].z - cz;
            const double rel = (r2 - (dx * dx + dy * dy + dz * dz)) / (r2 + 1e-12);
            EXPECT_LE(rel, 1e-5) << "vertex " << vi << " lies inside a tet circumsphere "
                                    "(empty-sphere Delaunay property violated)";
        }
    }
    return total;
}

} // namespace

TEST(TetDelaunay3D, TooFewPointsProduceNothing) {
    EXPECT_TRUE(TetDelaunay3D::compute({}).empty());
    EXPECT_TRUE(TetDelaunay3D::compute({{0, 0, 0}}).empty());
    std::vector<Vec3> three = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
    EXPECT_TRUE(TetDelaunay3D::compute(three).empty());
}

TEST(TetDelaunay3D, SingleTetrahedron) {
    std::vector<Vec3> four = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    TetMesh m = TetDelaunay3D::compute(four);
    ASSERT_EQ(m.tetrahedronCount(), 1u);
    EXPECT_NEAR(expectHealthy(m, four), 1.0 / 6.0, 1e-6);
}

TEST(TetDelaunay3D, InteriorPointSplitsIntoFourTets) {
    // An outer tetrahedron plus its interior centroid: Delaunay connects the centroid to
    // each of the four faces, tiling the same volume with four tets.
    std::vector<Vec3> five = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1},
                              {0.25f, 0.25f, 0.25f}};
    TetMesh m = TetDelaunay3D::compute(five);
    ASSERT_EQ(m.tetrahedronCount(), 4u);
    EXPECT_NEAR(expectHealthy(m, five), 1.0 / 6.0, 1e-6);
}

TEST(TetDelaunay3D, CubeCornersTileTheUnitCube) {
    // Eight cube corners are maximally cospherical — the classic stress case for a float
    // in-sphere test. A correct result tiles the unit cube: total volume exactly 1.
    std::vector<Vec3> cube = {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0},
                              {0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}};
    TetMesh m = TetDelaunay3D::compute(cube);
    ASSERT_GE(m.tetrahedronCount(), 5u);  // a cube tetrahedralizes into 5 or 6 tets
    EXPECT_NEAR(expectHealthy(m, cube), 1.0, 1e-5);
}

TEST(TetDelaunay3D, CubeCornersPlusInteriorTileVolumeExactly) {
    // Hull is exactly the unit cube, so any valid tetrahedralization must tile volume 1
    // with no overlaps (would exceed 1) or gaps (would fall short) — a strong overlap/gap
    // check, on top of the empty-sphere property.
    std::vector<Vec3> pts = {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0},
                             {0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}};
    std::mt19937 rng(77);
    std::uniform_real_distribution<float> d(0.05f, 0.95f);
    for (int i = 0; i < 30; ++i) pts.push_back({d(rng), d(rng), d(rng)});

    TetMesh m = TetDelaunay3D::compute(pts);
    EXPECT_NEAR(expectHealthy(m, pts), 1.0, 1e-5);
}

TEST(TetDelaunay3D, RandomPointSetsAreValidDelaunay) {
    for (unsigned seed : {1u, 7u, 42u, 1234u}) {
        std::mt19937 rng(seed);
        std::uniform_real_distribution<float> d(0.f, 1.f);
        std::vector<Vec3> pts;
        for (int i = 0; i < 50; ++i) pts.push_back({d(rng), d(rng), d(rng)});

        TetMesh m = TetDelaunay3D::compute(pts);
        ASSERT_FALSE(m.empty()) << "seed " << seed;
        const double vol = expectHealthy(m, pts);
        // The hull sits inside the unit cube and, for 50 uniform points, fills most of it.
        EXPECT_GT(vol, 0.4) << "seed " << seed;
        EXPECT_LT(vol, 1.0 + 1e-6) << "seed " << seed;
    }
}

TEST(TetDelaunay3D, ComputeIsDeterministic) {
    std::mt19937 rng(999);
    std::uniform_real_distribution<float> d(0.f, 1.f);
    std::vector<Vec3> pts;
    for (int i = 0; i < 30; ++i) pts.push_back({d(rng), d(rng), d(rng)});

    TetMesh a = TetDelaunay3D::compute(pts);
    TetMesh b = TetDelaunay3D::compute(pts);
    ASSERT_EQ(a.tetrahedronCount(), b.tetrahedronCount());
    for (size_t i = 0; i < a.tetrahedra.size(); ++i)
        EXPECT_EQ(a.tetrahedra[i], b.tetrahedra[i]) << "tet " << i;
}

// The property the fixed-shape fixtures above could not see: a Delaunay tetrahedralization
// tiles the convex hull of its input EXACTLY, for any input, not just for well-conditioned
// ones. Both volumes are accumulated in double from the same float coordinates through the
// orientation predicate, so a correct result matches to near machine precision.
TEST(TetDelaunay3D, TilesTheConvexHullOfRandomInputExactly) {
    auto hullVolume = [](const std::vector<Vec3>& pts) {
        const ConvexHull h = MeshConvexHull::build(pts);
        if (h.faces.empty()) return 0.0;
        double six = 0.0;
        for (const auto& f : h.faces)
            six += RobustPredicates::orient3D(h.vertices[f[0]], h.vertices[f[1]],
                                              h.vertices[f[2]], Vec3{0.f, 0.f, 0.f});
        return std::abs(six) / 6.0;
    };

    std::mt19937 rng(555u);
    std::uniform_real_distribution<float> u(-1.f, 1.f);
    std::uniform_int_distribution<int> gi(-3, 3);

    int checked = 0;
    for (int it = 0; it < 12; ++it) {
        std::vector<std::pair<const char*, std::vector<Vec3>>> families;
        {   // uniform in a cube
            std::vector<Vec3> p;
            for (int i = 0; i < 25; ++i) p.push_back({u(rng), u(rng), u(rng)});
            families.emplace_back("uniform", std::move(p));
        }
        {   // a 1e-4-thick slab — the sliver case the super-tetrahedron sizing exists for
            std::vector<Vec3> p;
            for (int i = 0; i < 22; ++i) p.push_back({u(rng), u(rng), u(rng) * 1e-4f});
            families.emplace_back("near-planar", std::move(p));
        }
        {   // integer grid: exactly cospherical quadruples are common
            std::vector<Vec3> p;
            for (int i = 0; i < 25; ++i)
                p.push_back({(float)gi(rng), (float)gi(rng), (float)gi(rng)});
            families.emplace_back("grid", std::move(p));
        }
        {   // a tight cluster beside a few far points
            std::vector<Vec3> p;
            for (int i = 0; i < 18; ++i)
                p.push_back({u(rng) * 1e-3f, u(rng) * 1e-3f, u(rng) * 1e-3f});
            for (int i = 0; i < 4; ++i) p.push_back({u(rng), u(rng), u(rng)});
            families.emplace_back("clustered", std::move(p));
        }

        for (const auto& [name, pts] : families) {
            const TetMesh m = TetDelaunay3D::compute(pts);
            const double hv = hullVolume(pts);
            if (hv <= 1e-12) continue;  // degenerate input has no tetrahedralization
            ASSERT_FALSE(m.empty()) << name << " it=" << it << ": no tetrahedra for a solid input";

            double vol = 0.0;
            for (const auto& t : m.tetrahedra)
                vol += tetVolume(m.vertices[t[0]], m.vertices[t[1]],
                                 m.vertices[t[2]], m.vertices[t[3]]);
            ++checked;
            EXPECT_NEAR(vol, hv, hv * 1e-6)
                << name << " it=" << it << ": tetrahedra cover " << vol << " of a hull of " << hv
                << " (" << 100.0 * (hv - vol) / hv << "% missing)";
        }
    }
    EXPECT_GT(checked, 30) << "the sweep did not run";
}
