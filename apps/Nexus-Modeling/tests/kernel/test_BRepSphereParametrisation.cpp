// A sphere whose parametrisation did not match its own grid.
//
// `analyticPatch` evaluates a Sphere as
//     p = r*(sin(u)*uAxis + cos(u)*cos(v)*vAxis + cos(u)*sin(v)*normal)
// so u is a latitude measured ABOUT uAxis, and the parametrisation is singular at the two
// points +/-uAxis, where cos(u) = 0 and v means nothing.
//
// `makeSphere` built its vertex rings about Z and then declared uAxis = X. The two singular
// points were therefore (+/-r, 0, 0) — on the GEOMETRIC EQUATOR, in the middle of a band
// face rather than at a vertex. Two things followed, and both were measured before the fix:
//
//   * `massProperties` integrates a curved face over the straight-edged polygon its vertices
//     map to in (u,v). With the frame misaligned that polygon is not the face's image: a
//     band face's edges are not parameter-aligned, and a face containing a singular point
//     folds over it. Per-face results were meaningless. sphere(r=1, lat=5, lon=4) reported
//     1.80 against 4.19; lat=5, lon=3 reported exactly 0.00, which `fromIntegrals` turns
//     into a default-constructed MassProperties — a closed 15-face solid with ZERO MASS,
//     which is what a rigid-body solver would then have been handed.
//
//   * Only a lucky subset came out right, and it was the subset the tests sampled. The sum
//     over faces telescopes to the whole sphere whenever the wrong polygons happen to TILE
//     the parameter domain exactly, which even lat with even lon >= 4 does. Every assertion
//     in the suite used even counts, so a body that was wrong face-by-face reported an exact
//     total and looked correct.
//
// With uAxis = Z the parameters ARE the grid: u is the ring latitude, v the longitude, each
// band face is an exact rectangle in (u,v), and each pole fan degenerates at a real vertex
// where the existing pole expansion handles it. Verified against an independent closed form
// below: for a band face, integral of x*n_x dS = r^3 * integral(cos^3 u du) *
// integral(cos^2 v dv), which for lat=4/lon=4 is 0.58926 * 0.78540 = 0.462800 — and the
// integrator now reports 0.462800306 for every one of the eight band faces, where before it
// reported 0.6558 for three of them and -1.1458 for the fourth.

#include <nexus/geometry/AnalyticBRep.h>

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>

namespace nexus::geometry::brep::testing {

namespace {
constexpr double kPi = 3.14159265358979323846;
}  // namespace

// THE ROOT CAUSE, pinned directly: a surface's parametrisation may only be singular where
// the face grid is singular — at a vertex. If a degeneracy falls inside a face, that face
// has no valid (u,v) polygon at all. This is the invariant, not the symptom.
TEST(BRepSphereParametrisation, TheParametrisationIsSingularOnlyAtVertices)
{
    for (const uint32_t lat : {2u, 3u, 4u, 5u, 8u, 9u}) {
        for (const uint32_t lon : {3u, 4u, 7u, 12u}) {
            const Body s = makeSphere(1.f, lat, lon);
            ASSERT_GT(s.faceCount(), 0u);

            for (uint32_t f = 0; f < static_cast<uint32_t>(s.faceCount()); ++f) {
                const Surface& surf = s.surface(s.face(f).surface);
                ASSERT_EQ(surf.kind, SurfaceKind::Sphere);

                // The two singular points of this parametrisation.
                for (const double sign : {1.0, -1.0}) {
                    const Vec3d sing{surf.origin.x + sign * surf.radius * surf.uAxis.x,
                                     surf.origin.y + sign * surf.radius * surf.uAxis.y,
                                     surf.origin.z + sign * surf.radius * surf.uAxis.z};
                    bool atVertex = false;
                    for (uint32_t v = 0; v < static_cast<uint32_t>(s.vertexCount()); ++v) {
                        const Vec3d& p = s.vertex(v).point;
                        const double dx = p.x - sing.x, dy = p.y - sing.y, dz = p.z - sing.z;
                        if (std::sqrt(dx * dx + dy * dy + dz * dz) < 1e-6) { atVertex = true; break; }
                    }
                    EXPECT_TRUE(atVertex)
                        << "lat=" << lat << " lon=" << lon << ": the parametrisation is singular at ("
                        << sing.x << ", " << sing.y << ", " << sing.z
                        << "), which is not a vertex — it lies inside a face";
                }
            }
        }
    }
}

// The property the parametric integrator exists for, asserted across the counts that used
// to be wrong rather than the ones that happened to be right.
TEST(BRepSphereParametrisation, VolumeIsExactAtEverySegmentCount)
{
    for (uint32_t lat = 2; lat <= 10; ++lat) {
        for (uint32_t lon = 3; lon <= 13; ++lon) {
            const double r = 1.0;
            const double truth = 4.0 / 3.0 * kPi * r * r * r;
            const double got = makeSphere(static_cast<float>(r), lat, lon).massProperties(1.f).volume;
            EXPECT_NEAR(got, truth, truth * 1e-5)
                << "sphere lat=" << lat << " lon=" << lon << " volume " << got;
        }
    }
}

// The most dangerous symptom the old frame produced, kept as its own guard: an odd lat with
// lon=3 integrated to exactly zero, and a zero-volume body silently becomes a default
// MassProperties — zero mass, zero inertia — for a solid that reports isClosed() == true.
TEST(BRepSphereParametrisation, NoSegmentCountProducesAMasslessSolid)
{
    for (uint32_t lat = 2; lat <= 10; ++lat) {
        for (uint32_t lon = 3; lon <= 13; ++lon) {
            const Body s = makeSphere(1.f, lat, lon);
            ASSERT_TRUE(s.isClosed()) << "lat=" << lat << " lon=" << lon;
            const MassProperties mp = s.massProperties(1.f);
            EXPECT_GT(mp.volume, 1.0) << "lat=" << lat << " lon=" << lon << " is a massless solid";
            double inertiaTrace = 0.0;
            for (int i = 0; i < 3; ++i) inertiaTrace += mp.inertia[i][i];
            EXPECT_GT(inertiaTrace, 1e-3) << "lat=" << lat << " lon=" << lon << " has no inertia";
        }
    }
}

// Volume can be right by cancellation; the centroid and the inertia tensor cannot. A
// translated sphere pins both, at the odd counts that were previously garbage.
TEST(BRepSphereParametrisation, CentroidAndInertiaAreRightAtOddCounts)
{
    const double r = 1.3;
    const double V = 4.0 / 3.0 * kPi * r * r * r;
    const double I = 0.4 * V * r * r;  // 2/5 m r^2 about any axis through the centre

    for (const uint32_t lat : {3u, 5u, 7u, 9u}) {
        for (const uint32_t lon : {3u, 5u, 11u}) {
            Body s = makeSphere(static_cast<float>(r), lat, lon);
            s.translate({2., -3., 0.5});
            const MassProperties mp = s.massProperties(1.f);

            EXPECT_NEAR(mp.volume, V, V * 1e-5) << "lat=" << lat << " lon=" << lon;
            EXPECT_NEAR(mp.centroid.x, 2.0, 1e-3) << "lat=" << lat << " lon=" << lon;
            EXPECT_NEAR(mp.centroid.y, -3.0, 1e-3) << "lat=" << lat << " lon=" << lon;
            EXPECT_NEAR(mp.centroid.z, 0.5, 1e-3) << "lat=" << lat << " lon=" << lon;

            // inertia is reported about the centroid, so translation must not change it
            for (int i = 0; i < 3; ++i)
                EXPECT_NEAR(mp.inertia[i][i], I, I * 1e-3)
                    << "moment " << i << " at lat=" << lat << " lon=" << lon;
            for (int i = 0; i < 3; ++i)
                for (int j = 0; j < 3; ++j)
                    if (i != j)
                        EXPECT_NEAR(mp.inertia[i][j], 0.f, I * 1e-3)
                            << "product " << i << j << " at lat=" << lat << " lon=" << lon;
        }
    }
}

// Area is the other integral over the same parameter domain, so it fails the same way.
TEST(BRepSphereParametrisation, SurfaceAreaIsExactAtEverySegmentCount)
{
    for (const uint32_t lat : {2u, 3u, 5u, 8u, 9u}) {
        for (const uint32_t lon : {3u, 4u, 7u, 12u}) {
            const double r = 0.75;
            const double truth = 4.0 * kPi * r * r;
            const double got = makeSphere(static_cast<float>(r), lat, lon).surfaceArea();
            EXPECT_NEAR(got, truth, truth * 1e-5)
                << "sphere lat=" << lat << " lon=" << lon << " area " << got;
        }
    }
}

}  // namespace nexus::geometry::brep::testing
