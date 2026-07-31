// When an imprint cuts a face along a circle, two arcs share the cut's endpoints and
// exactly one of them lies inside the face. Picking the wrong one is the quietest defect
// in this kernel: the complement reproduces the SAME endpoint vertices, so checkGeometry,
// checkIntegrity, isClosed and Euler are all satisfied by it. Nothing but the arc's own
// SPAN distinguishes them — which is the lesson the curved-face imprint recorded, applied
// here to the selector rather than the imprint.
//
// The selector used to test one candidate and flip if it came back outside. That is only
// sound while the containment test is trustworthy, and for a curved face it is not always:
// containment on a curved patch is decided in the surface's (u,v) domain, and an analytic
// sphere's parametrisation is SINGULAR at ±uAxis, where v is an atan2 that has no value.
// makeSphere puts uAxis on X while its tessellation's grid poles are on Z, so that
// singularity lands in the middle of ordinary patches near ±X — precisely where a box's
// ±X faces cut their seam.
//
// Measured on box(2³) against sphere(r1.2, 8x12): of 72 seam arcs, 71 discriminated
// cleanly and ONE reported BOTH candidates outside. Flipping on that answer chose an arc
// spanning 6.0333 rad where the truth is 0.2499 — 96% of the circle, the long way round.
//
// The damage was not local. That one arc swallowed ten ring vertices belonging to its
// neighbours, so the pass that reconciles the two operands' discretizations split it at
// each of them, and `splitEdge` manufactured a fresh vertex at ten positions where the
// sphere already had one. Ten duplicate pairs, far below any weld band; the Boolean's sew
// then collapsed them, every seam edge acquired a third user, and fromFaces correctly
// refused. Box against sphere returned empty for all three operators.
//
// The tests below assert the three layers separately — no complement arc, no duplicate
// vertex, and the Boolean that both were preventing — because each failed silently on its
// own and only the last one was visible from outside.

#include <nexus/geometry/AnalyticBRep.h>
#include <nexus/geometry/BRepBoolean.h>
#include <nexus/geometry/BRepSurfaceIntersect.h>
#include <nexus/geometry/MeshMassProperties.h>

#include <gtest/gtest.h>

#include <cmath>

namespace nexus::geometry::brep::testing {

namespace {

constexpr double kTwoPi = 6.283185307179586476925286766559;

Body boxAndSphereImprinted(double radius, double dx, Body& sphereOut)
{
    Body box = makeBox(2.f, 2.f, 2.f);
    Body sph = makeSphere(static_cast<float>(radius), 8, 12);
    sph.translate({dx, 0., 0.});
    (void)imprintMutually(box, sph);
    sphereOut = sph;
    return box;
}

// Cut the sphere with the box's seam circles ONE WAY, to a fixpoint — the first half of
// what imprintMutually does. The complement arc has to be caught HERE: a later pass
// reconciles the two operands' discretizations and subdivides the offending arc into
// eleven ordinary-looking pieces, so by the time the mutual imprint returns, no single
// edge spans the long way round any more and only the duplicate vertices it left behind
// are still visible. Asserting the span after imprintMutually silently checks nothing.
Body sphereImprintedOneWay(double radius, double dx)
{
    Body box = makeBox(2.f, 2.f, 2.f);
    Body sph = makeSphere(static_cast<float>(radius), 8, 12);
    sph.translate({dx, 0., 0.});
    for (int pass = 0; pass < 8; ++pass) {
        bool any = false;
        for (uint32_t fb = 0; fb < static_cast<uint32_t>(box.faceCount()); ++fb) {
            if (!box.face(fb).alive) continue;
            for (uint32_t f = 0; f < static_cast<uint32_t>(sph.faceCount()); ++f) {
                if (!sph.face(f).alive) continue;
                const auto si = intersectSurfaces(box.surface(box.face(fb).surface),
                                                  sph.surface(sph.face(f).surface));
                if (si.kind != SurfaceIntersectionKind::Circle) continue;
                if (sph.imprintCurve(f, si.curve) != kInvalid) any = true;
            }
        }
        if (!any) break;
    }
    return sph;
}

// The angle between an edge's own endpoints, measured the SHORT way round its circle.
double shortWayBetweenEndpoints(const Body& b, uint32_t e, const Curve& c)
{
    const Vec3 p0 = b.vertex(b.edge(e).v0).point, p1 = b.vertex(b.edge(e).v1).point;
    const Vec3 d0{p0.x - c.origin.x, p0.y - c.origin.y, p0.z - c.origin.z};
    const Vec3 d1{p1.x - c.origin.x, p1.y - c.origin.y, p1.z - c.origin.z};
    const double n0 = std::sqrt(d0.x * d0.x + d0.y * d0.y + d0.z * d0.z);
    const double n1 = std::sqrt(d1.x * d1.x + d1.y * d1.y + d1.z * d1.z);
    if (n0 < 1e-12 || n1 < 1e-12) return -1.0;
    const double c01 = (d0.x * d1.x + d0.y * d1.y + d0.z * d1.z) / (n0 * n1);
    return std::acos(std::min(1.0, std::max(-1.0, c01)));
}

double meshVolume(const Body& b, uint32_t sub)
{
    return MeshMassProperties::compute(b.toMesh(sub)).volume;
}

}  // namespace

// THE headline. An arc that went the long way round still meets its own endpoints, so it
// is asserted by SPAN and nothing else.
TEST(BRepArcComplementSelection, NoImprintedArcTracesTheComplement)
{
    for (const double dx : {0.0, 0.3, 0.7}) {
        const Body sph = sphereImprintedOneWay(1.2, dx);

        int checked = 0, complements = 0;
        for (uint32_t e = 0; e < static_cast<uint32_t>(sph.edgeCount()); ++e) {
            if (!sph.edge(e).alive) continue;
            const uint32_t cu = sph.edge(e).curve;
            if (cu == kInvalid || sph.curve(cu).kind != CurveKind::Circle) continue;
            const double shortWay = shortWayBetweenEndpoints(sph, e, sph.curve(cu));
            if (shortWay < 1e-9) continue;  // a full circle carried by one edge is not a bite
            const double span = std::abs(sph.edge(e).t1 - sph.edge(e).t0);
            ++checked;
            if (std::abs(span - (kTwoPi - shortWay)) < 1e-6) {
                ++complements;
                EXPECT_TRUE(false) << "dx=" << dx << ": edge " << e << " spans " << span
                                   << " rad but its endpoints are " << shortWay
                                   << " rad apart — it traces the complement, the long way "
                                      "round, and every validator accepts it";
            }
        }
        EXPECT_GT(checked, 0) << "dx=" << dx << ": no arc edges to check — fixture is wrong";
        EXPECT_EQ(complements, 0) << "dx=" << dx;
    }
}

// The consequence: a complement arc swallows its neighbours' ring vertices, and the
// reconcile pass then re-creates them. Two vertices closer than the sew's weld band are
// two vertices the sew cannot keep apart.
TEST(BRepArcComplementSelection, ImprintLeavesNoVerticesInsideTheWeldBand)
{
    constexpr double kWeld = 1e-4;  // booleanToBody's band at the default tolerance
    for (const double dx : {0.0, 0.3, 0.7}) {
        Body sph;
        const Body box = boxAndSphereImprinted(1.2, dx, sph);

        const Body* const bodies[] = {&box, &sph};
        for (const Body* body : bodies) {
            int pairs = 0;
            double closest = 1e9;
            for (uint32_t i = 0; i < static_cast<uint32_t>(body->vertexCount()); ++i) {
                if (!body->vertex(i).alive) continue;
                for (uint32_t j = i + 1; j < static_cast<uint32_t>(body->vertexCount()); ++j) {
                    if (!body->vertex(j).alive) continue;
                    const Vec3 a = body->vertex(i).point, b = body->vertex(j).point;
                    const double d = std::sqrt((a.x - b.x) * (a.x - b.x) +
                                               (a.y - b.y) * (a.y - b.y) +
                                               (a.z - b.z) * (a.z - b.z));
                    closest = std::min(closest, d);
                    if (d < kWeld) ++pairs;
                }
            }
            EXPECT_EQ(pairs, 0) << "dx=" << dx << ": " << pairs
                                << " distinct vertex pairs sit inside the weld band (closest "
                                << closest << ") — the sew cannot tell them apart";
        }
    }
}

// What the two defects above were preventing. Correctness is inclusion–exclusion against
// the MUTUALLY IMPRINTED operands, since the result carries the seam vertices and the
// pristine operands do not.
TEST(BRepArcComplementSelection, CentredBoxSphereBooleansAreWatertightAndConserveVolume)
{
    const Body A = makeBox(2.f, 2.f, 2.f);
    const Body B = makeSphere(1.2f, 8, 12);

    const Body U = booleanToBody(A, B, BooleanOp::Union);
    const Body I = booleanToBody(A, B, BooleanOp::Intersection);
    const Body D = booleanToBody(A, B, BooleanOp::Difference);

    for (const auto& [nm, r] :
         {std::pair{"union", &U}, {"intersection", &I}, {"difference", &D}}) {
        ASSERT_GT(r->faceCount(), 0u) << nm << " came back empty";
        EXPECT_TRUE(r->isClosed()) << nm << " is not watertight";
        EXPECT_TRUE(r->checkIntegrity().ok) << nm;
        EXPECT_TRUE(r->checkGeometry().ok) << nm;
    }

    Body Ai = A, Bi = B;
    ASSERT_TRUE(imprintMutually(Ai, Bi));
    for (const uint32_t sub : {0u, 2u}) {
        const double lhs = meshVolume(U, sub) + meshVolume(I, sub);
        const double rhs = meshVolume(Ai, sub) + meshVolume(Bi, sub);
        EXPECT_NEAR(lhs, rhs, 1e-5 * rhs) << "sub" << sub << ": U+I=" << lhs << " A+B=" << rhs;
    }
}

// A sphere strictly inside the box has answers that can be named exactly, which no volume
// tolerance can fudge: the union IS the box, the intersection IS the sphere, and the
// difference is the box carrying a spherical void (two shells, so more faces than either).
TEST(BRepArcComplementSelection, SphereContainedInBoxGivesTheExactOperands)
{
    const Body A = makeBox(2.f, 2.f, 2.f);
    const Body B = makeSphere(0.8f, 8, 12);  // corner distance is 1.732, so strictly inside

    const Body U = booleanToBody(A, B, BooleanOp::Union);
    const Body I = booleanToBody(A, B, BooleanOp::Intersection);
    const Body D = booleanToBody(A, B, BooleanOp::Difference);

    EXPECT_EQ(U.faceCount(), A.faceCount()) << "union of a box with a sphere inside it is the box";
    EXPECT_EQ(I.faceCount(), B.faceCount())
        << "intersection of a box with a sphere inside it is the sphere";
    EXPECT_EQ(D.faceCount(), A.faceCount() + B.faceCount())
        << "difference should be the box's skin plus the void's";

    for (const Body* r : {&U, &I, &D}) {
        EXPECT_TRUE(r->isClosed());
        EXPECT_TRUE(r->checkIntegrity().ok);
        EXPECT_TRUE(r->checkGeometry().ok);
    }
    EXPECT_NEAR(meshVolume(U, 2), meshVolume(A, 2), 1e-9);
    EXPECT_NEAR(meshVolume(I, 2), meshVolume(B, 2), 1e-9);
}

// PERMANENT: the invariant has to hold across configurations that still bail. Offset and
// large-radius box/sphere pairs do not all sew yet, and the contract is that such a case
// returns a clean empty Body — never a leaky or corrupt one.
TEST(BRepArcComplementSelection, WatertightOrEmptyAcrossOffsetsAndRadii)
{
    for (const double dx : {0.0, 0.3, 0.5, 0.7, 1.0}) {
        for (const double R : {0.8, 1.2, 1.6}) {
            Body B = makeSphere(static_cast<float>(R), 8, 12);
            B.translate({dx, 0., 0.});
            const Body A = makeBox(2.f, 2.f, 2.f);
            for (const BooleanOp op :
                 {BooleanOp::Union, BooleanOp::Intersection, BooleanOp::Difference}) {
                const Body r = booleanToBody(A, B, op);
                const bool ok = r.faceCount() == 0u || (r.isClosed() && r.checkIntegrity().ok);
                EXPECT_TRUE(ok) << "dx=" << dx << " R=" << R << " op=" << static_cast<int>(op)
                                << " produced a leaky body (" << r.faceCount() << " faces)";
            }
        }
    }
}

}  // namespace nexus::geometry::brep::testing
