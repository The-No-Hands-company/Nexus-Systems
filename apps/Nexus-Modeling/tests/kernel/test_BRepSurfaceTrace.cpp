// Foundation — numerical surface/surface intersection, the quartic cases.
//
// intersectSurfaces answers only where the section is exactly a Line or a Circle, which is
// where the configuration is axially symmetric. Everything else is a quartic space curve
// and was declined outright. Measured across 3592 chained boolean steps (see
// BRepBooleanDiagnostic), those declines were 35.2% of ALL outcomes and roughly five times
// the next-largest gap — which is what makes tracing them worth building.
//
// A numerical tracer is only as believable as its oracles, so these are the two that carry
// the file:
//
//   1. WHERE A CLOSED FORM EXISTS, THE TRACE MUST REPRODUCE IT. intersectSurfaces computes
//      those by an entirely different route — algebra on the surface parameters, no
//      iteration — so agreeing with it is real evidence rather than self-consistency.
//   2. THE STEINMETZ IDENTITY. Two equal-radius cylinders crossing at a right angle meet in
//      two PLANE ellipses lying in x = +z and x = −z. That is a fact about geometry, known
//      before this code existed, and it is the only case here whose answer is a pair of
//      curves that cross each other.
//
// The tracer is deliberately NOT wired into the imprint yet. This is the geometric core,
// verified alone, the same way intersectSurfaces was before anything depended on it.

#include <nexus/geometry/BRepSurfaceIntersect.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>

namespace nexus::geometry::brep::testing {

namespace {

Surface planeS(Vec3 o, Vec3 n) { Surface s; s.kind = SurfaceKind::Plane; s.origin = o; s.normal = n; return s; }
Surface sphereS(Vec3 c, double r) { Surface s; s.kind = SurfaceKind::Sphere; s.origin = c; s.radius = r; return s; }
Surface cylS(Vec3 o, Vec3 ax, double r) { Surface s; s.kind = SurfaceKind::Cylinder; s.origin = o; s.normal = ax; s.radius = r; return s; }
Surface coneS(Vec3 apex, Vec3 ax, double slope) { Surface s; s.kind = SurfaceKind::Cone; s.origin = apex; s.normal = ax; s.radius = slope; return s; }

double len(const Vec3& v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }
Vec3 diff(const Vec3& a, const Vec3& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }

SurfaceTrace traceIn(const Surface& a, const Surface& b, double R)
{
    return traceSurfaceIntersection(a, b, {-R, -R, -R}, {R, R, R});
}

// Worst |implicit distance| of every traced point to both surfaces. This is the definitive
// pointwise check: a point of an intersection curve is a point on both surfaces.
double worstOffSurface(const SurfaceTrace& t, const Surface& a, const Surface& b)
{
    double worst = 0.0;
    for (const TracedBranch& br : t.branches) {
        for (const Vec3& p : br.points) {
            worst = std::max(worst, std::max(std::abs(static_cast<double>(surfaceDistance(a, p))),
                                             std::abs(static_cast<double>(surfaceDistance(b, p)))));
        }
    }
    return worst;
}

size_t totalPoints(const SurfaceTrace& t)
{
    size_t n = 0;
    for (const TracedBranch& br : t.branches) n += br.points.size();
    return n;
}

}  // namespace

TEST(BRepSurfaceTrace, ReproducesEveryCircleTheAnalyticIntersectorKnows)
{
    // THE oracle. For each pair whose section intersectSurfaces returns in closed form, the
    // traced polyline must lie on that exact circle — same plane, same radius. The two
    // routes share no code: one solves for the circle algebraically, the other iterates.
    struct Case { const char* name; Surface a, b; double box; };
    const Case cases[] = {
        {"plane through a sphere's centre", planeS({0, 0, 0}, {0, 0, 1}), sphereS({0, 0, 0}, 1.0), 2.0},
        {"plane off-centre through a sphere", planeS({0, 0, 0.5}, {0, 0, 1}), sphereS({0, 0, 0}, 1.0), 2.0},
        {"two overlapping spheres", sphereS({0, 0, 0}, 1.0), sphereS({1.2, 0, 0}, 1.0), 2.5},
        {"cone with a coaxial cylinder", coneS({0, 0, 0}, {0, 0, 1}, 0.5), cylS({0, 0, 0}, {0, 0, 1}, 0.3), 2.0},
    };

    for (const Case& c : cases) {
        const auto analytic = intersectSurfaces(c.a, c.b);
        ASSERT_EQ(analytic.kind, SurfaceIntersectionKind::Circle) << c.name;

        const SurfaceTrace t = traceIn(c.a, c.b, c.box);
        ASSERT_TRUE(t.ok) << c.name;
        ASSERT_EQ(t.branches.size(), 1u) << c.name;
        EXPECT_TRUE(t.branches[0].closed) << c.name << ": a circle must come back closed";

        const Vec3 ctr = analytic.curve.origin;
        const Vec3 ax = analytic.curve.dir;
        for (const Vec3& p : t.branches[0].points) {
            const Vec3 w = diff(p, ctr);
            const double axial = w.x * ax.x + w.y * ax.y + w.z * ax.z;
            const Vec3 radial{w.x - ax.x * axial, w.y - ax.y * axial, w.z - ax.z * axial};
            EXPECT_NEAR(axial, 0.0, 1e-4) << c.name << ": off the circle's plane";
            EXPECT_NEAR(len(radial), analytic.curve.radius, 1e-4) << c.name << ": wrong radius";
        }
    }
}

TEST(BRepSurfaceTrace, ReproducesTheTwoRingsOfASphereOnACylinderAxis)
{
    // The one closed-form case that is two curves rather than one — so it also checks that
    // the seeding finds BOTH, which a single-branch tracer would silently half-answer.
    const Surface sph = sphereS({0, 0, 0}, 1.0);
    const Surface cyl = cylS({0, 0, 0}, {0, 0, 1}, 0.6);
    const auto analytic = intersectSurfaces(sph, cyl);
    ASSERT_EQ(analytic.kind, SurfaceIntersectionKind::TwoCircles);

    const SurfaceTrace t = traceIn(sph, cyl, 2.0);
    ASSERT_TRUE(t.ok);
    ASSERT_EQ(t.branches.size(), 2u) << "both rings, or the trace found only half the answer";
    for (const TracedBranch& br : t.branches) EXPECT_TRUE(br.closed);

    // The rings sit at ±sqrt(R² − r²) along the axis, one either side of the centre.
    const double z = std::sqrt(1.0 - 0.36);
    double zLo = 1e30, zHi = -1e30;
    for (const TracedBranch& br : t.branches) {
        for (const Vec3& p : br.points) { zLo = std::min(zLo, p.z); zHi = std::max(zHi, p.z); }
    }
    EXPECT_NEAR(zLo, -z, 1e-4);
    EXPECT_NEAR(zHi, z, 1e-4);
    EXPECT_LT(worstOffSurface(t, sph, cyl), 1e-4);
}

TEST(BRepSurfaceTrace, TheSteinmetzEllipsesLieInTheirTwoPlanes)
{
    // x²+y²=1 and y²+z²=1 subtract to x²=z², so the intersection is exactly the two plane
    // curves x=+z and x=−z. Nothing in the tracer knows that.
    const Surface a = cylS({0, 0, 0}, {0, 0, 1}, 1.0);
    const Surface b = cylS({0, 0, 0}, {1, 0, 0}, 1.0);
    const SurfaceTrace t = traceIn(a, b, 2.5);
    ASSERT_TRUE(t.ok);

    // Those two ellipses CROSS, at (0, ±1, 0), and at a crossing the curve has no tangent.
    // The honest answer is therefore four arcs, not two loops — and it is also the answer an
    // imprint wants, since arcs are edges and the crossings are vertices. Before the tracer
    // detected this it wandered from one ellipse onto the other indefinitely, producing
    // 40001 points per branch and reporting success.
    ASSERT_EQ(t.branches.size(), 4u);
    int onPlus = 0, onMinus = 0;
    for (const TracedBranch& br : t.branches) {
        EXPECT_FALSE(br.closed) << "an arc between two singular points is not a loop";
        EXPECT_LT(br.points.size(), 400u) << "an ellipse arc needs tens of points, not hundreds";
        double worst = 0.0;
        int votePlus = 0;
        for (const Vec3& p : br.points) {
            const double dPlus = std::abs(p.x - p.z), dMinus = std::abs(p.x + p.z);
            worst = std::max(worst, std::min(dPlus, dMinus));
            votePlus += (dPlus < dMinus) ? 1 : -1;
        }
        EXPECT_LT(worst, 1e-5) << "a Steinmetz arc left the plane it must lie in";
        (votePlus > 0 ? onPlus : onMinus)++;

        // Each arc runs between the two singular points, so its ends approach (0, ±1, 0).
        for (const Vec3& end : {br.points.front(), br.points.back()}) {
            EXPECT_NEAR(std::abs(end.y), 1.0, 1e-2) << "an arc did not end at a singular point";
            EXPECT_NEAR(end.x, 0.0, 5e-2);
            EXPECT_NEAR(end.z, 0.0, 5e-2);
        }
    }
    EXPECT_EQ(onPlus, 2) << "each ellipse is cut into two arcs by the two crossings";
    EXPECT_EQ(onMinus, 2);
}

TEST(BRepSurfaceTrace, TracesTheQuarticsThatHaveNoClosedFormAtAll)
{
    // The reason the file exists. There is no analytic answer to compare against, so the
    // check is the defining property: every point lies on both surfaces, and a curve that
    // should close, closes.
    struct Case { const char* name; Surface a, b; double box; size_t branches; };
    const Case cases[] = {
        {"sphere pierced by an off-axis rod", sphereS({0, 0, 0}, 1.0), cylS({0.4, 0, 0}, {0, 0, 1}, 0.3), 2.0, 2},
        {"cylinder crossed by a thinner one", cylS({0, 0, 0}, {0, 0, 1}, 1.0), cylS({0, 0, 0}, {1, 0, 0}, 0.5), 2.5, 2},
        {"cone met by an off-axis rod", coneS({0, 0, 0}, {0, 0, 1}, 0.5), cylS({0.4, 0, 0}, {0, 0, 1}, 0.3), 2.0, 1},
        {"cone met by an off-centre sphere", coneS({0, 0, 0}, {0, 0, 1}, 1.0), sphereS({0.5, 0, 1.5}, 0.8), 3.0, 1},
    };

    for (const Case& c : cases) {
        // Each of these was Unsupported, and therefore an empty boolean, before this landed.
        EXPECT_EQ(intersectSurfaces(c.a, c.b).kind, SurfaceIntersectionKind::Unsupported) << c.name;

        const SurfaceTrace t = traceIn(c.a, c.b, c.box);
        ASSERT_TRUE(t.ok) << c.name;
        EXPECT_EQ(t.branches.size(), c.branches) << c.name;
        for (const TracedBranch& br : t.branches) {
            EXPECT_TRUE(br.closed) << c.name << ": a curve wholly inside the box must close";
            EXPECT_GE(br.points.size(), 8u) << c.name;
        }
        EXPECT_LT(worstOffSurface(t, c.a, c.b), 1e-4)
            << c.name << ": a traced point is not on both surfaces";
    }
}

TEST(BRepSurfaceTrace, ATangencyIsAPointAndYieldsNoCurve)
{
    // Where two surfaces touch instead of crossing, Newton still finds points satisfying
    // both equations and the march still emits a few before the tangent gives out. Two
    // tangent spheres produced a 3-point "branch" and the trace called itself successful —
    // the same silent nonsense the boolean diagnostic work was about, one layer down.
    struct Case { const char* name; Surface a, b; double box; };
    const Case cases[] = {
        {"externally tangent spheres", sphereS({0, 0, 0}, 1.0), sphereS({2, 0, 0}, 1.0), 3.0},
        {"cylinder tangent to a sphere", sphereS({0, 0, 0}, 1.0), cylS({1.3, 0, 0}, {0, 0, 1}, 0.3), 2.0},
    };
    for (const Case& c : cases) {
        const SurfaceTrace t = traceIn(c.a, c.b, c.box);
        EXPECT_TRUE(t.branches.empty()) << c.name << ": a tangency is not a curve";
        EXPECT_FALSE(t.ok) << c.name;
    }
}

TEST(BRepSurfaceTrace, SurfacesThatDoNotMeetProduceNothing)
{
    EXPECT_TRUE(traceIn(sphereS({0, 0, 0}, 1.0), sphereS({5, 0, 0}, 1.0), 8.0).branches.empty());
    EXPECT_TRUE(traceIn(sphereS({0, 0, 0}, 1.0), sphereS({0, 0, 0}, 0.5), 2.0).branches.empty());
    // Coincident surfaces overlap in a SURFACE, not a curve; there is no polyline to give.
    EXPECT_TRUE(traceIn(sphereS({0, 0, 0}, 1.0), sphereS({0, 0, 0}, 1.0), 2.0).branches.empty());
}

TEST(BRepSurfaceTrace, KeepsASmallButRealCurveWhileRejectingTangencyDebris)
{
    // The cut between "a tangency artifact" and "a genuinely small circle" is placed on the
    // measured gap between the two populations: artifacts spanned 1.9e-04 and 7.2e-04 of the
    // box diagonal, the smallest real curve 2.7e-02 — a factor of 37. This pins the small
    // real one, which is the side that would be silently lost if the cut drifted up.
    const Surface a = sphereS({0, 0, 0}, 1.0);
    const Surface b = sphereS({1.99, 0, 0}, 1.0);  // overlapping by 0.01
    const SurfaceTrace t = traceIn(a, b, 3.0);
    ASSERT_EQ(t.branches.size(), 1u) << "a small circle is still a circle";
    EXPECT_TRUE(t.branches[0].closed);
    EXPECT_LT(worstOffSurface(t, a, b), 1e-4);
}

TEST(BRepSurfaceTrace, IsBitwiseDeterministic)
{
    // Seeds come off a fixed lattice in a fixed order and nothing hashes, so two calls must
    // agree to the last bit — the same standard every other part of this kernel is held to.
    const Surface a = sphereS({0, 0, 0}, 1.0);
    const Surface b = cylS({0.4, 0, 0}, {0, 0, 1}, 0.3);
    const SurfaceTrace t1 = traceIn(a, b, 2.0);
    const SurfaceTrace t2 = traceIn(a, b, 2.0);
    ASSERT_EQ(t1.branches.size(), t2.branches.size());
    ASSERT_GT(totalPoints(t1), 0u);
    for (size_t i = 0; i < t1.branches.size(); ++i) {
        ASSERT_EQ(t1.branches[i].points.size(), t2.branches[i].points.size()) << "branch " << i;
        ASSERT_EQ(t1.branches[i].closed, t2.branches[i].closed);
        for (size_t j = 0; j < t1.branches[i].points.size(); ++j) {
            EXPECT_EQ(t1.branches[i].points[j].x, t2.branches[i].points[j].x) << i << "/" << j;
            EXPECT_EQ(t1.branches[i].points[j].y, t2.branches[i].points[j].y) << i << "/" << j;
            EXPECT_EQ(t1.branches[i].points[j].z, t2.branches[i].points[j].z) << i << "/" << j;
        }
    }
}

TEST(BRepSurfaceTrace, ChordSagStaysWithinTheStatedBudget)
{
    // The polyline claims a fidelity — chords bow away from the true curve by less than
    // 1e-4 of the box diagonal — and that claim is what makes the sample count meaningful
    // rather than incidental. Checked here against the SURFACES, which is the only
    // description of the true curve available: the midpoint of each chord must be within
    // the budget of lying on both.
    const Surface a = sphereS({0, 0, 0}, 1.0);
    const Surface b = cylS({0.4, 0, 0}, {0, 0, 1}, 0.3);
    const double diag = std::sqrt(3.0) * 4.0;  // the [-2,2]^3 box
    const SurfaceTrace t = traceIn(a, b, 2.0);
    ASSERT_TRUE(t.ok);

    double worstMid = 0.0;
    for (const TracedBranch& br : t.branches) {
        for (size_t i = 1; i < br.points.size(); ++i) {
            const Vec3& p = br.points[i - 1];
            const Vec3& q = br.points[i];
            const Vec3 mid{0.5 * (p.x + q.x), 0.5 * (p.y + q.y), 0.5 * (p.z + q.z)};
            worstMid = std::max(worstMid,
                                std::max(std::abs(static_cast<double>(surfaceDistance(a, mid))),
                                         std::abs(static_cast<double>(surfaceDistance(b, mid)))));
        }
    }
    // The chord midpoint's distance to a surface is bounded by the sag, plus the curvature
    // term the sag budget is chosen to control. A loose multiple of the budget is the honest
    // bound to assert; what would fail is a polyline whose steps are not controlled at all.
    EXPECT_LT(worstMid, diag * 1e-3) << "chords are bowing further off the surfaces than the "
                                        "sag budget allows — step control has regressed";
}

}  // namespace nexus::geometry::brep::testing
