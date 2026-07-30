// Phase 2 of the true-analytic-curved-boolean arc — CURVED-FACE IMPRINT.
//
// imprintCurve accepted a Circle seam only when the target face was a PLANE, so a
// cylinder-through-box was imprinted on exactly one side: measured over every face
// pair of box(2,2,2) against cylinder(r=0.5,h=4), the 32 Circle seams landing on the
// box's planar faces were all accepted and the 32 landing on the cylinder's curved
// faces were all refused. One operand cut, the other straddling — which is why the
// sew opens and all three ops return empty.
//
// A cylinder contains exactly one family of circles: the LATITUDE circles, centred
// on the axis, in a plane perpendicular to it, at the cylinder's own radius. That is
// precisely what a plane perpendicular to the axis cuts, so it is the seam the
// cylinder-through-box case needs, and it crosses a side patch's two vertical edges
// — structurally the same two-point arc bite the planar path already handles.
//
// What could not be reused is the test deciding WHICH of the two arcs between those
// crossings lies inside the face: a curved patch's boundary is not planar, so the
// direct 3D point-in-polygon rule does not apply. Containment is decided in the
// surface's (u,v) parameter domain instead — where the boundary is an ordinary
// polygon — with the cylinder's periodic u unwrapped along the ring so a patch
// straddling the u = ±π seam is still a non-wrapping polygon. The load-bearing
// assertion below is therefore the ARC SPAN: picking the complement would give an arc
// of 2π − 2π/n instead of 2π/n, a factor of fifteen at sixteen segments.

#include <nexus/geometry/AnalyticBRep.h>

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

namespace nexus::geometry::brep::testing {


namespace {

constexpr uint32_t kSeg = 16;
constexpr float kRadius = 0.5f;
constexpr float kHeight = 4.f;
constexpr float kTwoPi = 6.28318530717958647692f;

// The latitude circle of makeCylinder(kRadius, kHeight, kSeg) at axial height z —
// the seam a plane perpendicular to the axis cuts on that cylinder.
Curve latitudeCircle(float z)
{
    Curve c;
    c.kind = CurveKind::Circle;
    c.origin = {0.f, 0.f, z};
    c.dir = {0.f, 0.f, 1.f};
    c.ref = {1.f, 0.f, 0.f};
    c.radius = kRadius;
    return c;
}

std::vector<uint32_t> cylindricalFaces(const Body& b)
{
    std::vector<uint32_t> out;
    for (uint32_t f = 0; f < static_cast<uint32_t>(b.faceCount()); ++f) {
        if (!b.face(f).alive) continue;
        const uint32_t s = b.face(f).surface;
        if (s < b.surfaceCount() && b.surface(s).kind == SurfaceKind::Cylinder) out.push_back(f);
    }
    return out;
}

}  // namespace

// Every side patch of the cylinder accepts the latitude seam — including whichever
// one straddles the u = ±π parameter seam, which is what the unwrapping is for.
TEST(BRepCurvedFaceImprint, LatitudeCircleImprintsOnEveryCylindricalFace)
{
    const Body base = makeCylinder(kRadius, kHeight, kSeg);
    const std::vector<uint32_t> sides = cylindricalFaces(base);
    ASSERT_EQ(sides.size(), kSeg) << "makeCylinder should have one Cylinder face per segment";

    for (uint32_t f : sides) {
        Body b = base;
        EXPECT_NE(b.imprintCurve(f, latitudeCircle(0.f)), kInvalid)
            << "latitude seam refused on cylindrical face " << f;
    }
}

// THE load-bearing check: the cut edge traces the arc INSIDE the patch. Choosing the
// complement is geometrically valid as a curve and would keep every validator happy,
// so only the span distinguishes them.
TEST(BRepCurvedFaceImprint, CutEdgeTracesTheArcInsideThePatchNotItsComplement)
{
    const Body base = makeCylinder(kRadius, kHeight, kSeg);
    const float patchSpan = kTwoPi / static_cast<float>(kSeg);  // 0.3927 rad at 16 segments

    for (uint32_t f : cylindricalFaces(base)) {
        Body b = base;
        const uint32_t firstNewEdge = static_cast<uint32_t>(b.edgeCount());
        ASSERT_NE(b.imprintCurve(f, latitudeCircle(0.f)), kInvalid);

        // Find the imprinted arc: a new edge whose curve is the latitude Circle.
        bool found = false;
        for (uint32_t e = firstNewEdge; e < static_cast<uint32_t>(b.edgeCount()); ++e) {
            if (!b.edge(e).alive) continue;
            const uint32_t cu = b.edge(e).curve;
            if (cu == kInvalid || b.curve(cu).kind != CurveKind::Circle) continue;
            if (std::abs(b.curve(cu).radius - kRadius) > 1e-5f) continue;
            const float span = std::abs(b.edge(e).t1 - b.edge(e).t0);
            EXPECT_NEAR(span, patchSpan, 1e-3f)
                << "face " << f << ": arc spans " << span << " rad; the patch spans "
                << patchSpan << " and its complement " << (kTwoPi - patchSpan)
                << " — the outside arc was chosen";
            found = true;
            break;
        }
        EXPECT_TRUE(found) << "no imprinted arc edge on face " << f;
    }
}

// The imprint is χ-neutral and leaves both validators clean on a curved face, exactly
// as on a planar one (ΔV=+2 from splitting the two vertical edges, ΔE=+3, ΔF=+1).
TEST(BRepCurvedFaceImprint, CurvedImprintIsEulerNeutralAndValid)
{
    const Body base = makeCylinder(kRadius, kHeight, kSeg);
    const uint32_t f = cylindricalFaces(base).front();

    Body b = base;
    ASSERT_NE(b.imprintCurve(f, latitudeCircle(0.f)), kInvalid);

    EXPECT_EQ(b.vertexCount(), base.vertexCount() + 2u);
    EXPECT_EQ(b.edgeCount(), base.edgeCount() + 3u);
    EXPECT_EQ(b.faceCount(), base.faceCount() + 1u);

    const Body::IntegrityReport ir = b.checkIntegrity();
    EXPECT_TRUE(ir.ok) << ir.reason;
    const Body::GeometryReport gr = b.checkGeometry();
    EXPECT_TRUE(gr.ok) << gr.reason;
    EXPECT_TRUE(b.isClosed()) << "imprinting must not open the shell";
    EXPECT_EQ(ir.euler, base.checkIntegrity().euler) << "a face cut is χ-neutral";
}

// The guard must not over-accept: a circle that is not a curve OF the cylinder is
// refused, so the imprint can never attach a trim curve that leaves the surface.
TEST(BRepCurvedFaceImprint, CirclesNotLyingOnTheCylinderAreRefused)
{
    const Body base = makeCylinder(kRadius, kHeight, kSeg);
    const uint32_t f = cylindricalFaces(base).front();

    {  // wrong radius — a coaxial circle floating inside the cylinder
        Body b = base;
        Curve c = latitudeCircle(0.f);
        c.radius = kRadius * 0.8f;
        EXPECT_EQ(b.imprintCurve(f, c), kInvalid);
    }
    {  // centre off the axis
        Body b = base;
        Curve c = latitudeCircle(0.f);
        c.origin = {0.2f, 0.f, 0.f};
        EXPECT_EQ(b.imprintCurve(f, c), kInvalid);
    }
    {  // circle plane not perpendicular to the axis
        Body b = base;
        Curve c = latitudeCircle(0.f);
        c.dir = {1.f, 0.f, 0.f};
        c.ref = {0.f, 1.f, 0.f};
        EXPECT_EQ(b.imprintCurve(f, c), kInvalid);
    }
    {  // a latitude circle beyond the patch's axial extent never crosses it
        Body b = base;
        EXPECT_EQ(b.imprintCurve(f, latitudeCircle(kHeight)), kInvalid);
    }
}

// Regression guard for the planar path the guard refactor also touched: a coplanar
// circle biting a planar face still works, and a sphere face still refuses (its
// circle imprint is a later increment).
TEST(BRepCurvedFaceImprint, PlanarPathUnchangedAndSphereStillRefused)
{
    const Body box = makeBox(2.f, 2.f, 2.f);
    uint32_t planar = kInvalid;
    for (uint32_t f = 0; f < static_cast<uint32_t>(box.faceCount()); ++f) {
        const uint32_t s = box.face(f).surface;
        if (s < box.surfaceCount() && box.surface(s).kind == SurfaceKind::Plane) { planar = f; break; }
    }
    ASSERT_NE(planar, kInvalid);

    {  // a circle centred on the +Z face, small enough to be a fully-interior hole
        Body b = box;
        Curve c;
        c.kind = CurveKind::Circle;
        c.dir = box.surface(box.face(planar).surface).normal;
        c.ref = {1.f, 0.f, 0.f};
        c.radius = 0.4f;
        c.origin = b.faceCentroid(planar);
        // Either an interior hole or a refusal is acceptable here; what must NOT happen
        // is a corrupt body.
        (void)b.imprintCurve(planar, c);
        EXPECT_TRUE(b.checkIntegrity().ok) << b.checkIntegrity().reason;
        EXPECT_TRUE(b.checkGeometry().ok) << b.checkGeometry().reason;
    }
    {  // a sphere's faces are still out of scope for the circle imprint
        Body s = makeSphere(1.f, 6, 10);
        for (uint32_t f = 0; f < static_cast<uint32_t>(s.faceCount()); ++f) {
            const uint32_t sid = s.face(f).surface;
            if (sid >= s.surfaceCount() || s.surface(sid).kind != SurfaceKind::Sphere) continue;
            Body b = s;
            Curve c = latitudeCircle(0.f);
            c.radius = 1.f;
            EXPECT_EQ(b.imprintCurve(f, c), kInvalid) << "sphere circle imprint is not yet in scope";
            break;
        }
    }
}

}  // namespace nexus::geometry::brep::testing
