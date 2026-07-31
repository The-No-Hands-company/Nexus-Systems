// A face that covered its own area twice, and the loop shape that did it.
//
// A seam imprinted twice leaves a SLIT: the second cut retraces the first, and the loop
// walks out along a chain of edges and back along a second chain lying on top of it. The
// two sides are distinct vertices at the same place — measured here at 2.2e-16, 1.1e-16
// and exactly 0.0 apart — so nothing in the B-rep objects. The body is closed, integral,
// checkGeometry-clean, and the imprint is idempotent.
//
// It is the TESSELLATION that cannot cope, because a loop like that is not a simple
// polygon and every triangulator downstream assumes one. The ear clipper skips a point
// that coincides with one of a candidate ear's corners — it has to, since the bridge it
// builds for holes walks in and out along the same cut — but it recognised those points by
// their INDEX. A slit's two sides have different indices, so each one sat on a candidate
// ear's corner and counted as blocking it. No ear was findable anywhere: the clipper
// stalled with 18 of 39 vertices left and fanned the remnant, and the face's triangles came
// to 0.1728 where its boundary encloses 0.0812. It covered itself twice over.
//
// That matters beyond area. classifyPoint casts a parity ray against this tessellation, so
// a doubly-covered region flips inside/outside for everything behind it — which is the
// same reason the arc-bite double-cover was worth fixing when it appeared.
//
// The repair splits the loop at the pinch, which is the standard reading of a self-touching
// polygon: at a repeated position i < j, [i,j) and [j,i) are each closed loops, and
// recursing separates every slit. Pieces enclosing no area are dropped; the rest are
// triangulated independently. On this fixture a ring of 39 becomes pieces of 18 and 9, with
// the remaining 12 points forming the zero-area slit between them — so BOTH pieces carry
// material and keeping only the largest would lose some.
//
// Fixing it upstream was tried first and rejected on measurement, which is worth recording
// so it is not tried again. Three rules were built and swept: refuse a cut whose span runs
// through any vertex of its own loop; refuse one that retraces an edge of that loop;
// shorten the cut to the first vertex it meets. The first two remove the slit. All three
// cost the same 5 of 2000 configurations losing every one of their three Booleans — and
// none of those 5 had any duplicate vertices at all, so the guards were refusing legitimate
// cuts. Splitting the loop where it is read costs nothing: 2101 sews before and after.

#include <nexus/geometry/AnalyticBRep.h>
#include <nexus/geometry/BRepBoolean.h>
#include <nexus/geometry/MeshMassProperties.h>

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <vector>

namespace nexus::geometry::brep::testing {

namespace {

// The recorded configuration (fuzz seed 0xA17E51, iteration 1781), rebuilt from the
// generator's own draws rather than described.
Body fixtureSphere()
{
    Body s = makeSphere(static_cast<float>(0.76880286526917851), 8, 12);
    nexus::render::Mat4 m = nexus::render::Mat4::identity();  // quarter turn about X
    m.m[1][1] = 0.f;
    m.m[1][2] = -1.f;
    m.m[2][1] = 1.f;
    m.m[2][2] = 0.f;
    (void)s.transform(m);
    return s;
}

Body fixtureBox()
{
    Body b = makeBox(static_cast<float>(1.5676656521988952),
                     static_cast<float>(0.50372912295032568),
                     static_cast<float>(1.7432088858841315));
    b.translate({-0.10221549264636787, -0.43950290650425272, 0.23524255429497853});
    return b;
}

// Unsigned area actually covered by a body's triangles.
double tessellatedArea(const Body& b, uint32_t sub)
{
    const Mesh m = b.toMesh(sub);
    const std::vector<nexus::render::Vec3>& P = m.attributes().positions();
    double a = 0.0;
    for (size_t f = 0; f < m.topology().faceCount(); ++f) {
        const std::vector<uint32_t>& idx = m.topology().face(f).indices;
        for (size_t k = 2; k < idx.size(); ++k) {
            const nexus::render::Vec3& p0 = P[idx[0]];
            const nexus::render::Vec3& p1 = P[idx[k - 1]];
            const nexus::render::Vec3& p2 = P[idx[k]];
            const double ux = p1.x - p0.x, uy = p1.y - p0.y, uz = p1.z - p0.z;
            const double vx = p2.x - p0.x, vy = p2.y - p0.y, vz = p2.z - p0.z;
            const double cx = uy * vz - uz * vy, cy = uz * vx - ux * vz, cz = ux * vy - uy * vx;
            a += 0.5 * std::sqrt(cx * cx + cy * cy + cz * cz);
        }
    }
    return a;
}

// Vertices that occupy the same point but are different vertices.
int coincidentVertexPairs(const Body& b)
{
    int n = 0;
    for (uint32_t i = 0; i < static_cast<uint32_t>(b.vertexCount()); ++i) {
        if (!b.vertex(i).alive) continue;
        for (uint32_t j = i + 1; j < static_cast<uint32_t>(b.vertexCount()); ++j) {
            if (!b.vertex(j).alive) continue;
            const Vec3 d = b.vertex(i).point - b.vertex(j).point;
            if (std::sqrt(d.dot(d)) < 1e-9) ++n;
        }
    }
    return n;
}

}  // namespace

// THE assertion, scoped to what this repair reaches. The imprinted box's triangles used to
// be inflated by the ear clipper stalling and fanning its remnant; they are not any more.
//
// It does NOT come out exactly equal to the box's six sides, and the residue is named
// rather than hidden: the slit means two DIFFERENT faces of the body cover the same
// material, which no change to the tessellator can fix — it is drawing faithfully what the
// B-rep holds. Splitting the loop removes the part of the excess that was one face drawn
// twice; the rest is the duplicated seam itself and is the subject of the upstream work the
// header comment describes.
TEST(BRepSlitLoopTessellation, TheStallInflationIsGoneAndTheResidueIsBounded)
{
    Body sph = fixtureSphere(), box = fixtureBox();
    ASSERT_TRUE(imprintMutually(sph, box));
    ASSERT_TRUE(box.isClosed());
    ASSERT_TRUE(box.checkIntegrity().ok);
    ASSERT_TRUE(box.checkGeometry().ok);

    const double sx = 1.5676656521988952, sy = 0.50372912295032568, sz = 1.7432088858841315;
    const double want = 2.0 * (sx * sy + sy * sz + sz * sx);  // 8.8011

    // Bounded on BOTH sides at each level, so neither a regression nor a silent fix can
    // pass unnoticed. The upper bounds are the measured values with this repair; the lower
    // bounds are what the stall produced before it (0.450 / 0.254 / 0.213 of excess).
    const double before[] = {0.450, 0.254, 0.213};
    const double after[] = {0.128, 0.163, 0.167};
    int i = 0;
    for (const uint32_t sub : {0u, 2u, 6u}) {
        const double excess = tessellatedArea(box, sub) - want;
        EXPECT_LT(excess, after[i])
            << "sub" << sub << ": excess area " << excess << " is above the measured "
            << after[i] << " — the stall inflation is back";
        EXPECT_LT(excess, before[i])
            << "sub" << sub << ": excess area " << excess
            << " is at or above the pre-repair " << before[i];
        EXPECT_GT(excess, 0.0)
            << "sub" << sub
            << ": the residual face overlap is gone — the duplicated seam must have been "
               "fixed upstream, so retire this bound and assert equality instead";
        ++i;
    }
}

// The loop shape that caused it, asserted where it lives. This is the condition the
// tessellator now handles; it is NOT asserted to be absent, because removing it upstream
// was measured to cost working Booleans (see the header comment).
TEST(BRepSlitLoopTessellation, TheFixtureReallyDoesCarryASlit)
{
    Body sph = fixtureSphere(), box = fixtureBox();
    ASSERT_TRUE(imprintMutually(sph, box));
    EXPECT_GT(coincidentVertexPairs(box), 0)
        << "the fixture no longer reproduces the slit, so the test above is not exercising "
           "the repair — find a configuration that does, or retire both";

    // and it is stable: imprinting again changes nothing, so this is one construction and
    // not an accumulation
    const size_t v = box.vertexCount();
    Body sph2 = sph, box2 = box;
    ASSERT_TRUE(imprintMutually(sph2, box2));
    EXPECT_EQ(box2.vertexCount(), v) << "the imprint is no longer idempotent";
}

// The Booleans on the fixture still behave: watertight-or-empty, and conserving.
TEST(BRepSlitLoopTessellation, TheFixtureBooleansStillSewAndConserve)
{
    const Body A = fixtureSphere(), B = fixtureBox();
    Body Ai = A, Bi = B;
    ASSERT_TRUE(imprintMutually(Ai, Bi));

    const Body U = booleanToBody(A, B, BooleanOp::Union);
    const Body I = booleanToBody(A, B, BooleanOp::Intersection);
    const Body D = booleanToBody(A, B, BooleanOp::Difference);
    for (const Body* r : {&U, &I, &D})
        EXPECT_TRUE(r->faceCount() == 0u ||
                    (r->isClosed() && r->checkIntegrity().ok && r->checkGeometry().ok))
            << "neither watertight nor empty";

    if (U.faceCount() > 0u && I.faceCount() > 0u) {
        auto vol = [](const Body& b, uint32_t s) {
            return MeshMassProperties::compute(b.toMesh(s)).volume;
        };
        for (const uint32_t sub : {0u, 2u}) {
            const double a = vol(Ai, sub), b = vol(Bi, sub);
            EXPECT_NEAR(vol(U, sub) + vol(I, sub), a + b, 1e-6 * (a + b))
                << "sub" << sub << ": U + I != A + B";
        }
    }
}

// A slit built directly, so the repair is pinned without depending on the imprint ever
// producing one again: a square whose loop walks out to an interior point and back. Its
// boundary encloses the square, and its triangles must cover exactly that.
TEST(BRepSlitLoopTessellation, AHandBuiltSlitIsTessellatedToItsTrueArea)
{
    // The outward chain and the return chain are DIFFERENT vertices at the same points,
    // which is what a doubly-imprinted seam produces. The chain must leave and re-enter the
    // boundary at the SAME corner — leaving at one corner and returning at another is a
    // notch, which is an ordinary concave polygon and exercises nothing (built that way
    // first: it tessellated to 3.5, correctly, because 3.5 is what that boundary encloses).
    std::vector<Vec3> pts{
        {0.0, 0.0, 0.0}, {2.0, 0.0, 0.0}, {2.0, 2.0, 0.0}, {0.0, 2.0, 0.0},  // 0..3 square
        {0.5, 1.0, 0.0}, {1.0, 1.0, 0.0},                                     // 4,5 outward
        {0.5, 1.0, 0.0}, {0.0, 2.0, 0.0},                                     // 6,7 return
    };
    Body::FaceDef fd;
    fd.loop = {0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u};  // ... corner 3, out to 5, back to corner 3
    fd.surface.kind = SurfaceKind::Plane;
    fd.surface.origin = {0., 0., 0.};
    fd.surface.normal = {0., 0., 1.};
    fd.surface.uAxis = {1., 0., 0.};
    const auto body = Body::fromFaces(pts, {fd});
    ASSERT_TRUE(body.has_value()) << "fixture did not build";
    ASSERT_EQ(body->faceCount(), 1u);

    EXPECT_NEAR(tessellatedArea(*body, 0), 4.0, 1e-9)
        << "the slit square's triangles do not cover its 2x2 boundary exactly once";
}

}  // namespace nexus::geometry::brep::testing
