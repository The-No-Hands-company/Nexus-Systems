// Phase 4a of the true-analytic-curved-boolean arc — A SHARED SEAM RING.
//
// With the driver wiring in place both operands of a cylinder-through-box get cut, but
// the sew still could not close, and the reason was resolution rather than position.
// A hole ring was built at a fixed eight arc segments while the cylinder discretized
// the SAME seam circle into sixteen. The rings agreed geometrically — the eight
// vertices sat on eight of the sixteen to 1.7e-7 — but each hole edge spanned two of
// the cylinder's, so no edge could ever find a partner.
//
// The imprint now accepts the other operand's vertices on the seam circle and builds
// the ring on exactly those points. That exposed an ORDERING problem, which is the more
// interesting half: a cylinder's latitude ring does not exist until the box has been
// imprinted onto the cylinder, so a box imprinted first has no partner ring to match.
// Committing to a resolution at that moment is precisely how the mismatch arose. So the
// hole defers when it is being coordinated across two operands and its partner ring is
// not there yet, and the mutual imprint runs a second round to make the deferred cuts.
// Everything already imprinted refuses idempotently, so the extra round converges.
//
// A caller imprinting a lone body passes no ring and still gets the uniform ring; that
// path is deliberately unchanged and is pinned below.

#include <nexus/geometry/AnalyticBRep.h>

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

namespace nexus::geometry::brep::testing {

using nexus::render::Vec3;

namespace {

constexpr float kCylR = 0.5f;
constexpr uint32_t kCylSeg = 16;

// Vertices of `b` lying on the circle of radius `r` about the Z axis at height `z`.
std::vector<Vec3> ringVertices(const Body& b, float z, float r)
{
    std::vector<Vec3> out;
    for (uint32_t i = 0; i < static_cast<uint32_t>(b.vertexCount()); ++i) {
        if (!b.vertex(i).alive) continue;
        const Vec3 p = b.vertex(i).point;
        if (std::abs(p.z - z) > 1e-3f) continue;
        if (std::abs(std::sqrt(p.x * p.x + p.y * p.y) - r) > 1e-3f) continue;
        out.push_back(p);
    }
    return out;
}

float worstNearestDistance(const std::vector<Vec3>& from, const std::vector<Vec3>& to)
{
    float worst = 0.f;
    for (const Vec3& p : from) {
        float best = 1e30f;
        for (const Vec3& q : to) {
            const Vec3 d = q - p;
            best = std::min(best, std::sqrt(d.dot(d)));
        }
        worst = std::max(worst, best);
    }
    return worst;
}

uint32_t firstPlanarFace(const Body& b)
{
    for (uint32_t f = 0; f < static_cast<uint32_t>(b.faceCount()); ++f) {
        if (!b.face(f).alive) continue;
        const uint32_t s = b.face(f).surface;
        if (s < b.surfaceCount() && b.surface(s).kind == SurfaceKind::Plane) return f;
    }
    return kInvalid;
}

// A circle lying in a planar face, small enough to fall wholly inside it.
Curve interiorCircleOn(const Body& b, uint32_t face, float radius)
{
    Curve c;
    c.kind = CurveKind::Circle;
    c.dir = b.surface(b.face(face).surface).normal;
    c.ref = {1.f, 0.f, 0.f};
    c.radius = radius;
    c.origin = b.faceCentroid(face);
    return c;
}

}  // namespace

// THE Phase 4a assertion: the two operands' rings on the same seam circle are the same
// ring — same count, same points. Before this they were 8 against 16.
TEST(BRepSharedSeamRing, OperandsShareTheSeamRingOnACylinderThroughBox)
{
    Body box = makeBox(2.f, 2.f, 2.f);
    Body cyl = makeCylinder(kCylR, 4.f, kCylSeg);  // pierces only the ±Z faces
    ASSERT_TRUE(imprintMutually(box, cyl));

    for (float z : {1.f, -1.f}) {
        const std::vector<Vec3> onBox = ringVertices(box, z, kCylR);
        const std::vector<Vec3> onCyl = ringVertices(cyl, z, kCylR);

        ASSERT_EQ(onCyl.size(), kCylSeg) << "cylinder latitude ring at z=" << z;
        EXPECT_EQ(onBox.size(), onCyl.size())
            << "at z=" << z << " the box's hole ring has " << onBox.size()
            << " vertices against the cylinder's " << onCyl.size()
            << " — the seam is discretized differently on the two sides, so no edge can "
               "partner";
        EXPECT_LT(worstNearestDistance(onBox, onCyl), 1e-6f)
            << "ring vertices do not coincide at z=" << z;
        EXPECT_LT(worstNearestDistance(onCyl, onBox), 1e-6f)
            << "a cylinder ring vertex has no counterpart on the box at z=" << z;
    }
}

// The shared ring must not cost validity: both operands stay sound, and the arcs built
// through the supplied points still reproduce their own endpoints (the ring's parameter
// ranges are derived per-point now, with the closing edge wrapping a full turn).
TEST(BRepSharedSeamRing, SharedRingKeepsBothOperandsValid)
{
    Body box = makeBox(2.f, 2.f, 2.f);
    Body cyl = makeCylinder(kCylR, 4.f, kCylSeg);
    ASSERT_TRUE(imprintMutually(box, cyl));

    EXPECT_TRUE(box.checkIntegrity().ok) << "box: " << box.checkIntegrity().reason;
    EXPECT_TRUE(box.checkGeometry().ok) << "box: " << box.checkGeometry().reason;
    EXPECT_TRUE(cyl.checkIntegrity().ok) << "cyl: " << cyl.checkIntegrity().reason;
    EXPECT_TRUE(cyl.checkGeometry().ok) << "cyl: " << cyl.checkGeometry().reason;
    EXPECT_TRUE(cyl.isClosed());
}

// The lone-body path is unchanged: no ring supplied means the uniform 8-segment ring,
// which is the only sensible answer when there is no partner to agree with.
TEST(BRepSharedSeamRing, StandaloneHoleImprintStillBuildsAUniformRing)
{
    Body b = makeBox(2.f, 2.f, 2.f);
    const uint32_t f = firstPlanarFace(b);
    ASSERT_NE(f, kInvalid);
    const size_t v0 = b.vertexCount();

    ASSERT_NE(b.imprintCurve(f, interiorCircleOn(b, f, 0.4f)), kInvalid);
    EXPECT_EQ(b.vertexCount(), v0 + 8u) << "the uniform hole ring should still be 8 segments";
    EXPECT_TRUE(b.checkIntegrity().ok) << b.checkIntegrity().reason;
    EXPECT_TRUE(b.checkGeometry().ok) << b.checkGeometry().reason;
}

// Supplied ring points are a HINT, never an authority: points that are not on the
// circle are discarded rather than placed as vertices off the curve.
TEST(BRepSharedSeamRing, RingPointsOffTheCircleAreIgnored)
{
    Body b = makeBox(2.f, 2.f, 2.f);
    const uint32_t f = firstPlanarFace(b);
    ASSERT_NE(f, kInvalid);
    const Curve c = interiorCircleOn(b, f, 0.4f);

    constexpr uint32_t kGood = 5;
    std::vector<Vec3> pts;
    for (uint32_t k = 0; k < kGood; ++k)
        pts.push_back(c.eval(6.28318530718f * static_cast<float>(k) / kGood));
    pts.push_back(c.origin);                                  // centre: not on the circle
    pts.push_back(c.eval(0.f) + Vec3{0.f, 0.f, 0.35f});       // off the circle's plane

    const size_t v0 = b.vertexCount();
    ASSERT_NE(b.imprintCurve(f, c, Tolerance{}, &pts), kInvalid);
    EXPECT_EQ(b.vertexCount(), v0 + kGood) << "an off-circle point became a ring vertex";
    EXPECT_TRUE(b.checkGeometry().ok) << b.checkGeometry().reason;
}

// Coordinated callers must not get an arbitrary resolution: when a ring list is passed
// and holds nothing usable, the hole is DEFERRED rather than guessed. This is the
// mechanism that lets the mutual imprint's second round build a matching ring.
TEST(BRepSharedSeamRing, CoordinatedHoleDefersWithoutAPartnerRing)
{
    Body b = makeBox(2.f, 2.f, 2.f);
    const uint32_t f = firstPlanarFace(b);
    ASSERT_NE(f, kInvalid);
    const size_t v0 = b.vertexCount();

    const std::vector<Vec3> none;
    EXPECT_EQ(b.imprintCurve(f, interiorCircleOn(b, f, 0.4f), Tolerance{}, &none), kInvalid)
        << "a coordinated hole with no partner ring must defer, not pick a resolution";
    EXPECT_EQ(b.vertexCount(), v0) << "the deferred hole still modified the body";
}

}  // namespace nexus::geometry::brep::testing
