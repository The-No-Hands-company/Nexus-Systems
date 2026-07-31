// Curved-boolean arc — CIRCLE-ONTO-SPHERE-FACE IMPRINT, and the seam ring it has to
// close before any sew can use it.
//
// Two things had to be true for a sphere to take a seam, and only the first was
// obvious.
//
// (1) A sphere had to be admitted at all. `circleLiesOnSurface` handled Plane and
// Cylinder and refused everything else, so every Circle seam offered to a spherical
// face was rejected at the guard and the sphere came out of the imprint untouched. A
// cylinder contains exactly one family of circles (the latitude circles); a sphere
// contains a two-parameter family — EVERY plane section — which is what a box face or
// a second sphere actually cuts. Its containment condition: writing d for
// centre-to-centre, |d + r·w|² is constant over the circle only when d lies along the
// circle's own axis, and the constant it takes is |d|² + r², which must equal R².
//
// (2) A crossing sitting exactly AT a boundary endpoint had to count. This is the same
// rule the straight-edge path already carried, and the reason it had to be learned
// twice is that on a cylinder it looks like a special case and on a sphere it is the
// general one: a cylinder's side patch is bounded by two straight uprights that carry
// the crossings, but a lat-lon patch is bounded by FOUR ARCS and has no straight edge
// anywhere. So every crossing on a sphere is an arc crossing, and the moment one patch
// is cut, its neighbours' shared boundary arcs are already split at that very point.
// Demanding a strictly interior fraction reports those neighbours UNCROSSED.
//
// The failure that causes is quiet, which is why the test below is a RING CENSUS rather
// than an acceptance count. Measured on box(2³)/sphere(r1.2, 8×12), each seam came out
// as 6 disconnected arc bites whose 12 endpoints were ALL degree 1 — and the body was
// closed, both validators were clean, and every vertex sat on the sphere to 2.2e-16.
// Nothing but the connectivity of the seam itself distinguishes a ring from confetti.
// Admitting the endpoint takes each seam to 12 edges over 12 vertices, every one
// degree 2.
//
// The payoff is the last test: SPHERE/SPHERE booleans now close. That pair was the
// oldest empty entry in the curved-boolean baseline.

#include <nexus/geometry/AnalyticBRep.h>
#include <nexus/geometry/BRepBoolean.h>
#include <nexus/geometry/MeshMassProperties.h>

#include <gtest/gtest.h>

#include <cmath>
#include <map>

namespace nexus::geometry::brep::testing {

namespace {

constexpr float kRadius = 1.2f;
constexpr uint32_t kLat = 8;
constexpr uint32_t kLon = 12;

// The circle a plane perpendicular to `axis`, `offset` from the centre, cuts on
// makeSphere(kRadius, ...) — the seam a box face or a second sphere produces.
// `axis` must be unit length; the arc's reference direction is derived by cross product
// rather than picked from a table, because for a TILTED axis a table entry is not
// actually perpendicular and the resulting frame describes some other curve entirely.
Curve sectionCircle(const Vec3& axis, double offset, double radiusOverride = -1.0)
{
    Curve c;
    c.kind = CurveKind::Circle;
    c.origin = {axis.x * offset, axis.y * offset, axis.z * offset};
    c.dir = axis;
    const Vec3 seed = (std::abs(axis.z) < 0.9) ? Vec3{0., 0., 1.} : Vec3{1., 0., 0.};
    const Vec3 r{axis.y * seed.z - axis.z * seed.y, axis.z * seed.x - axis.x * seed.z,
                 axis.x * seed.y - axis.y * seed.x};
    const double rl = std::sqrt(r.x * r.x + r.y * r.y + r.z * r.z);
    c.ref = {r.x / rl, r.y / rl, r.z / rl};
    const double r2 = static_cast<double>(kRadius) * kRadius - offset * offset;
    c.radius = (radiusOverride >= 0.0) ? radiusOverride : std::sqrt(r2);
    return c;
}

// Offer `seam` to every face until no face accepts it — what the imprint driver does,
// and what a full seam requires, since cutting one patch creates new patches.
uint32_t imprintEverywhere(Body& b, const Curve& seam)
{
    uint32_t accepted = 0;
    for (int pass = 0; pass < 8; ++pass) {
        const uint32_t before = accepted;
        for (uint32_t f = 0; f < static_cast<uint32_t>(b.faceCount()); ++f) {
            if (!b.face(f).alive) continue;
            if (b.imprintCurve(f, seam) != kInvalid) ++accepted;
        }
        if (accepted == before) break;
    }
    return accepted;
}

bool onCircle(const Vec3& p, const Curve& c, double eps)
{
    const Vec3 d{p.x - c.origin.x, p.y - c.origin.y, p.z - c.origin.z};
    const double axial = d.x * c.dir.x + d.y * c.dir.y + d.z * c.dir.z;
    const double dist = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
    return std::abs(axial) <= eps && std::abs(dist - c.radius) <= eps;
}

struct RingCensus {
    uint32_t edges = 0;
    uint32_t vertices = 0;
    uint32_t degreeTwo = 0;   // seam vertices used by exactly two seam edges
    uint32_t degreeOther = 0;
};

// Edges whose BOTH endpoints lie on the seam circle. A closed ring uses each of its
// vertices exactly twice; disconnected bites leave every endpoint at degree one.
RingCensus censusSeam(const Body& b, const Curve& seam, double eps = 1e-6)
{
    std::map<uint32_t, uint32_t> use;
    RingCensus rc;
    for (uint32_t e = 0; e < static_cast<uint32_t>(b.edgeCount()); ++e) {
        if (!b.edge(e).alive) continue;
        const uint32_t v0 = b.edge(e).v0, v1 = b.edge(e).v1;
        if (!onCircle(b.vertex(v0).point, seam, eps)) continue;
        if (!onCircle(b.vertex(v1).point, seam, eps)) continue;
        ++rc.edges;
        ++use[v0];
        ++use[v1];
    }
    rc.vertices = static_cast<uint32_t>(use.size());
    for (const auto& [v, n] : use) (n == 2 ? rc.degreeTwo : rc.degreeOther) += 1;
    return rc;
}

double meshVolume(const Body& b, uint32_t sub)
{
    return MeshMassProperties::compute(b.toMesh(sub)).volume;
}

Body sphereAt(double dx)
{
    Body b = makeSphere(kRadius, kLat, kLon);
    b.translate({dx, 0., 0.});
    return b;
}

}  // namespace

// A plane section of the sphere is accepted on its spherical faces — the capability the
// guard used to deny outright.
TEST(BRepSphereFaceImprint, PlaneSectionCircleIsAcceptedOnSphericalFaces)
{
    const double s2 = 1.0 / std::sqrt(2.0);
    const double s3 = 1.0 / std::sqrt(3.0);
    for (double offset : {0.3, 0.5, -0.5, 1.0}) {
        for (const Vec3& axis :
             {Vec3{0., 0., 1.}, Vec3{1., 0., 0.}, Vec3{s2, 0., s2}, Vec3{s3, s3, s3}}) {
            Body b = makeSphere(kRadius, kLat, kLon);
            EXPECT_GT(imprintEverywhere(b, sectionCircle(axis, offset)), 0u)
                << "no spherical face accepted the section at offset " << offset
                << " about axis (" << axis.x << "," << axis.y << "," << axis.z << ")";
        }
    }
}

// A section that coincides with the tessellation's OWN grid ring is refused, and that is
// correct rather than a gap: the sphere is already segmented along it, so there is no
// face for the circle to cross and nothing to cut. On an 8x12 sphere the great circles
// about x, y and z are exactly the grid's meridian pairs and its equator — which is why
// the offsets above avoid them, and why a tilted great circle (coinciding with no grid
// line) is accepted while an axis-aligned one is not.
TEST(BRepSphereFaceImprint, SectionCoincidingWithTheGridIsRefusedAsAlreadySegmented)
{
    for (const Vec3& axis : {Vec3{1., 0., 0.}, Vec3{0., 1., 0.}, Vec3{0., 0., 1.}}) {
        Body b = makeSphere(kRadius, kLat, kLon);
        const size_t f0 = b.faceCount();
        EXPECT_EQ(imprintEverywhere(b, sectionCircle(axis, 0.0)), 0u)
            << "a section lying along the grid's own edges was cut again";
        EXPECT_EQ(b.faceCount(), f0);
    }

    // the same great circle, tilted off the grid, IS cut — so the refusal above is about
    // coincidence with existing edges, not about great circles being unsupported
    const double s2 = 1.0 / std::sqrt(2.0);
    Body tilted = makeSphere(kRadius, kLat, kLon);
    EXPECT_GT(imprintEverywhere(tilted, sectionCircle({s2, 0., s2}, 0.0)), 0u);
    EXPECT_TRUE(tilted.isClosed());
    EXPECT_TRUE(tilted.checkGeometry().ok);
}

// THE load-bearing check. Acceptance counts, validators and watertightness all stayed
// clean while the seam was six disconnected bites; only its connectivity tells them
// apart. Asserted about BOTH a section aligned with the sphere's own pole axis (which
// crosses only meridians) and one perpendicular to it (which crosses meridians and
// parallels both), since only the second exercises arcs of two different families.
TEST(BRepSphereFaceImprint, SeamClosesIntoARingNotDisconnectedBites)
{
    const double s2 = 1.0 / std::sqrt(2.0);
    struct Case { Vec3 axis; double offset; const char* what; };
    const Case cases[] = {
        {{0., 0., 1.}, 1.0, "polar-aligned section at z=1"},
        {{0., 0., 1.}, 0.3, "polar-aligned section at z=0.3"},
        {{1., 0., 0.}, 1.0, "equator-aligned section at x=1"},
        {{1., 0., 0.}, -0.5, "equator-aligned section at x=-0.5"},
        {{s2, 0., s2}, 0.0, "tilted great circle, crossing arcs of both families"},
    };

    for (const Case& c : cases) {
        Body b = makeSphere(kRadius, kLat, kLon);
        const Curve seam = sectionCircle(c.axis, c.offset);
        ASSERT_GT(imprintEverywhere(b, seam), 0u) << c.what << ": seam was never accepted";

        const RingCensus rc = censusSeam(b, seam);
        EXPECT_EQ(rc.degreeOther, 0u)
            << c.what << ": " << rc.degreeOther << " of " << rc.vertices
            << " seam vertices are not shared by exactly two seam edges — the seam is "
               "disconnected bites, not a ring";
        EXPECT_EQ(rc.edges, rc.vertices)
            << c.what << ": " << rc.edges << " seam edges over " << rc.vertices
            << " seam vertices; a closed ring has one edge per vertex";
        EXPECT_GE(rc.edges, kLon) << c.what << ": a seam crossing " << kLon
                                  << " meridians cannot be built from " << rc.edges << " arcs";
    }
}

// Every seam vertex is on the sphere AND on the circle. A split placed by solving the
// wrong equation still yields a closed, valid body, so the position is asserted
// directly rather than inferred from the validators.
TEST(BRepSphereFaceImprint, SeamVerticesLieOnBothTheSphereAndTheCircle)
{
    Body b = makeSphere(kRadius, kLat, kLon);
    const Curve seam = sectionCircle({1., 0., 0.}, 1.0);
    const uint32_t firstNew = static_cast<uint32_t>(b.vertexCount());
    ASSERT_GT(imprintEverywhere(b, seam), 0u);
    ASSERT_GT(b.vertexCount(), firstNew) << "the imprint added no vertices";

    for (uint32_t v = firstNew; v < static_cast<uint32_t>(b.vertexCount()); ++v) {
        const Vec3 p = b.vertex(v).point;
        const double r = std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z);
        EXPECT_NEAR(r, static_cast<double>(kRadius), 1e-9)
            << "seam vertex " << v << " is not on the sphere";
        EXPECT_TRUE(onCircle(p, seam, 1e-6)) << "seam vertex " << v << " is not on the seam circle";
    }
}

// An imprint segments the boundary and never removes material: the body stays closed,
// both validators stay clean, and V-E+F stays 2 however many seams are cut.
TEST(BRepSphereFaceImprint, ImprintedSphereStaysClosedValidAndGenusZero)
{
    Body b = makeSphere(kRadius, kLat, kLon);
    ASSERT_EQ(b.vertexCount() - b.edgeCount() + b.faceCount(), 2u);

    // the six seams a 2x2x2 box centred on the sphere cuts
    for (const Vec3& axis : {Vec3{1., 0., 0.}, Vec3{0., 1., 0.}, Vec3{0., 0., 1.}}) {
        for (double offset : {1.0, -1.0}) {
            ASSERT_GT(imprintEverywhere(b, sectionCircle(axis, offset)), 0u);
            EXPECT_TRUE(b.isClosed()) << "the imprint opened the shell";
            EXPECT_TRUE(b.checkIntegrity().ok);
            EXPECT_TRUE(b.checkGeometry().ok);
            EXPECT_EQ(b.vertexCount() - b.edgeCount() + b.faceCount(), 2u)
                << "V-E+F left 2 — the imprint changed the topology, not just the segmentation";
        }
    }
}

// Re-offering a seam already imprinted must be refused. The driver re-offers every tool
// surface on every pass, so a path that keeps accepting its own output grows the body
// without bound rather than settling.
TEST(BRepSphereFaceImprint, ReofferingAnImprintedSeamIsRefused)
{
    Body b = makeSphere(kRadius, kLat, kLon);
    const Curve seam = sectionCircle({0., 0., 1.}, 1.0);
    ASSERT_GT(imprintEverywhere(b, seam), 0u);

    const size_t v = b.vertexCount(), e = b.edgeCount(), f = b.faceCount();
    EXPECT_EQ(imprintEverywhere(b, seam), 0u) << "the seam was cut a second time";
    EXPECT_EQ(b.vertexCount(), v);
    EXPECT_EQ(b.edgeCount(), e);
    EXPECT_EQ(b.faceCount(), f);
}

// Circles the sphere does not contain must still be refused — the guard must admit the
// section family without becoming a blanket yes for spheres.
TEST(BRepSphereFaceImprint, CirclesNotOnTheSphereAreRefused)
{
    // right axis and centre, wrong radius (the true section at z=0.5 has r=1.0909)
    const Curve wrongRadius = sectionCircle({0., 0., 1.}, 0.5, /*radiusOverride=*/0.9);
    // right radius for SOME section, but the centre is off the circle's own axis
    Curve offAxis = sectionCircle({0., 0., 1.}, 0.5);
    offAxis.origin = {0.3, 0., 0.5};
    // a section of the wrong sphere entirely: correct form, radius too large for R
    const Curve tooBig = sectionCircle({0., 0., 1.}, 0.0, /*radiusOverride=*/2.0);

    for (const Curve& bad : {wrongRadius, offAxis, tooBig}) {
        Body b = makeSphere(kRadius, kLat, kLon);
        const size_t f0 = b.faceCount();
        EXPECT_EQ(imprintEverywhere(b, bad), 0u) << "a circle not on the sphere was imprinted";
        EXPECT_EQ(b.faceCount(), f0);
    }
}

// THE PAYOFF: sphere/sphere booleans close. Correctness is asserted as
// inclusion-exclusion — U + I must equal A + B — measured against the MUTUALLY
// IMPRINTED operands, because the boolean's output carries the seam vertices and the
// pristine operands do not, so comparing against those measures tessellation density
// rather than volume (it differs by ~5e-2, three orders above the real agreement).
TEST(BRepSphereFaceImprint, SphereSphereBooleansAreWatertightAndConserveVolume)
{
    for (double dx : {0.5, 1.0, 1.5}) {
        const Body A = makeSphere(kRadius, kLat, kLon);
        const Body B = sphereAt(dx);

        const Body U = booleanToBody(A, B, BooleanOp::Union);
        const Body I = booleanToBody(A, B, BooleanOp::Intersection);
        const Body D = booleanToBody(A, B, BooleanOp::Difference);

        for (const auto& [nm, r] : {std::pair{"union", &U}, {"intersection", &I},
                                    {"difference", &D}}) {
            ASSERT_GT(r->faceCount(), 0u) << nm << " at dx=" << dx << " came back empty";
            EXPECT_TRUE(r->isClosed()) << nm << " at dx=" << dx << " is not watertight";
            EXPECT_TRUE(r->checkIntegrity().ok) << nm << " at dx=" << dx;
            EXPECT_TRUE(r->checkGeometry().ok) << nm << " at dx=" << dx;
        }

        Body Ai = A, Bi = B;
        ASSERT_TRUE(imprintMutually(Ai, Bi));
        for (uint32_t sub : {0u, 2u}) {
            const double lhs = meshVolume(U, sub) + meshVolume(I, sub);
            const double rhs = meshVolume(Ai, sub) + meshVolume(Bi, sub);
            EXPECT_NEAR(lhs, rhs, 1e-5 * rhs)
                << "dx=" << dx << " sub" << sub << ": U+I=" << lhs << " but A+B=" << rhs;
        }
    }
}

}  // namespace nexus::geometry::brep::testing
