// Phase 4c of the true-analytic-curved-boolean arc — CLASSIFY A FACE BY ITS MATERIAL.
//
// A face is classified against the other solid by testing ONE point, and that point was
// faceCentroid: the average of the outer boundary's vertices. Take a box face pierced by
// a cylinder — a square with a circular hole through its middle. The average of the
// square's four corners is the centre of the square, which is the centre of the hole.
// The single point chosen to stand for the face is the one point the face does not
// occupy. It lies inside the cylinder, so the face was called Inside: dropped from the
// union, where its material is entirely OUTSIDE the cylinder and belongs on the
// boundary, and kept for the intersection, where none of it belongs.
//
// faceSamplePoint returns a point known to lie on the material instead — inside the
// outer boundary and inside none of the holes — and classifyFace samples that.
//
// Why not simply weight the centroid by area: for a hole CONCENTRIC with its face the
// area centroid is still the centre, because both regions share it. No formula that
// averages the face's extent can escape the hole, because the hole is where the middle
// is. That case is pinned below so the reasoning cannot be lost and quietly re-tried.

#include <nexus/geometry/BRepBoolean.h>

#include <gtest/gtest.h>

#include <cmath>

namespace nexus::geometry::brep::testing {

using PC = Body::PointContainment;

namespace {

constexpr float kCylR = 0.5f;

uint32_t firstPlanarFace(const Body& b)
{
    for (uint32_t f = 0; f < static_cast<uint32_t>(b.faceCount()); ++f) {
        if (!b.face(f).alive) continue;
        const uint32_t s = b.face(f).surface;
        if (s < b.surfaceCount() && b.surface(s).kind == SurfaceKind::Plane) return f;
    }
    return kInvalid;
}

double dist(const Vec3d& a, const Vec3d& b)
{
    const Vec3 d = a - b;
    return std::sqrt(d.dot(d));
}

}  // namespace

// The sample point's guarantee, stated for each kind of face it can be asked about.
//
// For a PLANAR face with no holes it IS the centroid, bit for bit, so every classification
// that predates the sample point is unchanged. That was once asserted for curved faces
// too, and it should not have been: the centroid of a ring of points on a curved surface
// does not lie on that surface — it sags inside by the chord-versus-arc difference — so
// what the assertion pinned was the sample point failing to be on its own face. It is now
// projected onto the surface, and the guarantee below is the stronger one: the point lies
// where it claims to.
TEST(BRepFaceSamplePoint, PlanarFacesKeepTheirCentroidAndCurvedOnesLieOnTheirSurface)
{
    for (const Body& b : {makeBox(2.f, 2.f, 2.f), makeCylinder(1.f, 2.f, 12),
                          makeSphere(1.f, 6, 10), makeCone(1.f, 2.f, 8)}) {
        for (uint32_t f = 0; f < static_cast<uint32_t>(b.faceCount()); ++f) {
            if (!b.face(f).alive) continue;
            ASSERT_TRUE(b.faceInnerLoopVertices(f).empty());
            const auto c = b.faceCentroid(f), s = b.faceSamplePoint(f);
            const Surface& surf = b.surface(b.face(f).surface);
            if (surf.kind == SurfaceKind::Plane) {
                EXPECT_FLOAT_EQ(s.x, c.x) << "face " << f;
                EXPECT_FLOAT_EQ(s.y, c.y) << "face " << f;
                EXPECT_FLOAT_EQ(s.z, c.z) << "face " << f;
                continue;
            }
            // on the surface, which the centroid it came from was not
            if (surf.kind == SurfaceKind::Sphere) {
                EXPECT_NEAR(dist(s, surf.origin), surf.radius, 1e-9) << "face " << f;
                EXPECT_GT(std::abs(dist(c, surf.origin) - surf.radius), 1e-9)
                    << "face " << f << ": the centroid was already on the sphere, so this "
                                       "fixture cannot show the difference";
            } else if (surf.kind == SurfaceKind::Cylinder) {
                const Vec3 d = s - surf.origin;
                const Vec3 ax = surf.normal;
                const Vec3 rad = d - ax * d.dot(ax);
                EXPECT_NEAR(std::sqrt(rad.dot(rad)), surf.radius, 1e-9) << "face " << f;
            }
        }
    }
}

// THE Phase 4c assertion: on a pierced face the centroid is in the opening and the
// sample point is on the material, and the two land on OPPOSITE sides of the piercing
// solid — which is precisely the classification that was wrong.
TEST(BRepFaceSamplePoint, PiercedFaceSamplesItsMaterialNotTheOpening)
{
    Body box = makeBox(2.f, 2.f, 2.f);
    Body cyl = makeCylinder(kCylR, 4.f, 16);
    ASSERT_TRUE(imprintMutually(box, cyl));

    size_t pierced = 0;
    for (uint32_t f = 0; f < static_cast<uint32_t>(box.faceCount()); ++f) {
        if (!box.face(f).alive || box.faceInnerLoopVertices(f).empty()) continue;
        ++pierced;
        EXPECT_EQ(cyl.classifyPoint(box.faceCentroid(f)), PC::Inside)
            << "face " << f << ": the centroid should fall in the opening";
        EXPECT_EQ(cyl.classifyPoint(box.faceSamplePoint(f)), PC::Outside)
            << "face " << f << ": the sample point is not on the face's material";
    }
    EXPECT_EQ(pierced, 2u) << "the cylinder should pierce the box's two ±Z faces";
}

// And therefore the face itself classifies correctly. The imprint segments each pierced
// ±Z face into a ring outside the cylinder and the disk inside it, and each half must be
// classified by the material it actually carries: every ring Outside (its centroid falls
// in the opening, which is what used to call it Inside), every disk Inside.
TEST(BRepFaceSamplePoint, PiercedBoxClassifiesEachSegmentByItsOwnMaterial)
{
    Body box = makeBox(2.f, 2.f, 2.f);
    Body cyl = makeCylinder(kCylR, 4.f, 16);
    ASSERT_TRUE(imprintMutually(box, cyl));

    size_t rings = 0, disks = 0;
    for (uint32_t f = 0; f < static_cast<uint32_t>(box.faceCount()); ++f) {
        if (!box.face(f).alive) continue;
        // A disk is the segment lying on the seam circle: it is the only kind of face here
        // whose whole boundary is arcs, every vertex of it at the cylinder's radius.
        const std::vector<uint32_t> vs = box.faceVertices(f);
        bool allOnCylinder = !vs.empty();
        for (uint32_t v : vs) {
            const auto& p = box.vertex(v).point;
            if (std::fabs(std::sqrt(p.x * p.x + p.y * p.y) - kCylR) > 1e-3f) {
                allOnCylinder = false;
                break;
            }
        }
        if (allOnCylinder) {
            ++disks;
            EXPECT_EQ(box.classifyFace(f, cyl), PC::Inside)
                << "box face " << f << " is the disk inside the cylinder";
        } else {
            if (!box.faceInnerLoopVertices(f).empty()) ++rings;
            EXPECT_EQ(box.classifyFace(f, cyl), PC::Outside)
                << "box face " << f << " has no material inside the cylinder, so it cannot be "
                   "classified Inside";
        }
    }
    EXPECT_EQ(disks, 2u) << "the cylinder should leave a disk in each of the box's ±Z faces";
    EXPECT_EQ(rings, 2u) << "and the remainder of each of those faces is a ring with a hole";
}

// The case that rules out every averaging scheme, pinned so the reasoning survives: for
// a hole concentric with its face, the outline average AND the area-weighted centroid
// are both the hole's centre. Only a sampled point escapes.
TEST(BRepFaceSamplePoint, ConcentricHoleDefeatsAnyAveragedCentroid)
{
    Body b = makeBox(2.f, 2.f, 2.f);
    const uint32_t f = firstPlanarFace(b);
    ASSERT_NE(f, kInvalid);

    const auto centre = b.faceCentroid(f);
    constexpr float holeR = 0.4f;
    Curve c;
    c.kind = CurveKind::Circle;
    c.dir = b.surface(b.face(f).surface).normal;
    c.ref = {1.f, 0.f, 0.f};
    c.radius = holeR;
    c.origin = centre;  // concentric with the face
    ASSERT_NE(b.imprintCurve(f, c), kInvalid);

    // The centroid is unmoved by the hole and sits exactly at its centre — distance 0,
    // so no area weighting could push it out either.
    EXPECT_LT(dist(b.faceCentroid(f), centre), 1e-6f)
        << "the outline average should be unchanged by punching a concentric hole";

    // The sample point is outside the hole's radius, hence on the material.
    const auto s = b.faceSamplePoint(f);
    EXPECT_GT(dist(s, centre), holeR)
        << "the sample point is inside the concentric hole (distance " << dist(s, centre)
        << " < radius " << holeR << ")";
    // …and still within the face's own extent.
    EXPECT_LE(std::abs(s.x), 1.f + 1e-5f);
    EXPECT_LE(std::abs(s.y), 1.f + 1e-5f);
    EXPECT_LE(std::abs(s.z), 1.f + 1e-5f);
}

// Determinism: the same body must yield the same sample point every time, because a
// classification that varies between runs would break reproducibility on the Null path.
TEST(BRepFaceSamplePoint, SamplePointIsDeterministic)
{
    for (int repeat = 0; repeat < 3; ++repeat) {
        Body box = makeBox(2.f, 2.f, 2.f);
        Body cyl = makeCylinder(kCylR, 4.f, 16);
        ASSERT_TRUE(imprintMutually(box, cyl));

        Body box2 = makeBox(2.f, 2.f, 2.f);
        Body cyl2 = makeCylinder(kCylR, 4.f, 16);
        ASSERT_TRUE(imprintMutually(box2, cyl2));

        ASSERT_EQ(box.faceCount(), box2.faceCount());
        for (uint32_t f = 0; f < static_cast<uint32_t>(box.faceCount()); ++f) {
            if (!box.face(f).alive) continue;
            EXPECT_LT(dist(box.faceSamplePoint(f), box2.faceSamplePoint(f)), 1e-9f)
                << "face " << f << " on repeat " << repeat;
        }
    }
}

}  // namespace nexus::geometry::brep::testing
