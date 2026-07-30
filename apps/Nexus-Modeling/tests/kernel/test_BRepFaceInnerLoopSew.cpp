// Phase 4b of the true-analytic-curved-boolean arc — INNER LOOPS THROUGH THE SEW.
//
// A boolean seam that pierces the middle of a face — a cylinder driven through a box's
// face — leaves that face bounded outside and holed inside. The representation has
// always supported this (checkIntegrity, toMesh and serialization all handle inner
// loops, and imprintCurve creates them), but the ASSEMBLY path could not express one:
// faceVertices reports the outer boundary alone, FaceDef held a single ring, fromFaces
// built only outer loops, and the boolean's face collection never looked at innerLoops.
// So a hole was silently discarded on the way out and the pierced face was reassembled
// solid — losing the opening, and leaving the other operand's ring edges with nothing
// to partner against.
//
// This closes that: FaceDef carries inner rings, fromFaces builds them through the same
// edge-dedup and coedge-partnering path as an outer ring (a hole differs only in winding
// and in being listed as inner, so it must not get a second implementation),
// faceInnerLoopVertices reads them back, and the boolean carries them across the weld —
// reversing each hole when the face is flipped outward, so it keeps winding against the
// boundary rather than with it.

#include <nexus/geometry/AnalyticBRep.h>

#include <gtest/gtest.h>

#include <vector>

namespace nexus::geometry::brep::testing {


namespace {

uint32_t firstPlanarFace(const Body& b)
{
    for (uint32_t f = 0; f < static_cast<uint32_t>(b.faceCount()); ++f) {
        if (!b.face(f).alive) continue;
        const uint32_t s = b.face(f).surface;
        if (s < b.surfaceCount() && b.surface(s).kind == SurfaceKind::Plane) return f;
    }
    return kInvalid;
}

// A box with one face holed by an interior circle — the shape a pierced boolean face has.
Body boxWithAHoledFace(uint32_t& holedFace)
{
    Body b = makeBox(2.f, 2.f, 2.f);
    holedFace = firstPlanarFace(b);
    if (holedFace == kInvalid) return b;
    Curve c;
    c.kind = CurveKind::Circle;
    c.dir = b.surface(b.face(holedFace).surface).normal;
    c.ref = {1.f, 0.f, 0.f};
    c.radius = 0.4f;
    c.origin = b.faceCentroid(holedFace);
    if (b.imprintCurve(holedFace, c) == kInvalid) holedFace = kInvalid;
    return b;
}

// Disassemble every face of `b` into FaceDefs (outer ring + holes) and reassemble.
// This is exactly the round trip the boolean's sew performs.
std::optional<Body> roundTrip(const Body& b)
{
    std::vector<Vec3d> pts;
    pts.reserve(b.vertexCount());
    for (uint32_t v = 0; v < static_cast<uint32_t>(b.vertexCount()); ++v)
        pts.push_back(b.vertex(v).point);

    std::vector<Body::FaceDef> defs;
    for (uint32_t f = 0; f < static_cast<uint32_t>(b.faceCount()); ++f) {
        if (!b.face(f).alive) continue;
        Body::FaceDef fd;
        fd.loop = b.faceVertices(f);
        if (fd.loop.size() < 3) continue;
        fd.surface = b.surface(b.face(f).surface);
        fd.innerLoops = b.faceInnerLoopVertices(f);
        defs.push_back(std::move(fd));
    }
    return Body::fromFaces(pts, defs);
}

}  // namespace

// The reader: a holed face reports its hole, and reports it separately from the outer
// boundary rather than merged into it.
TEST(BRepFaceInnerLoopSew, HoledFaceReportsItsInnerLoop)
{
    uint32_t f = kInvalid;
    const Body b = boxWithAHoledFace(f);
    ASSERT_NE(f, kInvalid) << "the interior-circle imprint should have holed a face";

    EXPECT_EQ(b.faceVertices(f).size(), 4u) << "the outer boundary is still the box face's quad";

    const std::vector<std::vector<uint32_t>> holes = b.faceInnerLoopVertices(f);
    ASSERT_EQ(holes.size(), 1u) << "the hole was not reported";
    EXPECT_EQ(holes[0].size(), 8u) << "the uniform hole ring is 8 segments";

    // Every other face is unholed, so the reader must not invent loops.
    for (uint32_t g = 0; g < static_cast<uint32_t>(b.faceCount()); ++g)
        if (g != f && b.face(g).alive)
            EXPECT_TRUE(b.faceInnerLoopVertices(g).empty()) << "face " << g;
}

// THE Phase 4b assertion: a face with a hole survives assembly. Before this, fromFaces
// had nowhere to put the ring and the face came back solid.
TEST(BRepFaceInnerLoopSew, InnerLoopSurvivesAssembly)
{
    uint32_t f = kInvalid;
    const Body src = boxWithAHoledFace(f);
    ASSERT_NE(f, kInvalid);

    const std::optional<Body> out = roundTrip(src);
    ASSERT_TRUE(out.has_value()) << "reassembly rejected a face with a hole";

    size_t holedFaces = 0, holeRings = 0, holeVerts = 0;
    for (uint32_t g = 0; g < static_cast<uint32_t>(out->faceCount()); ++g) {
        if (!out->face(g).alive) continue;
        const std::vector<std::vector<uint32_t>> holes = out->faceInnerLoopVertices(g);
        if (holes.empty()) continue;
        ++holedFaces;
        holeRings += holes.size();
        for (const std::vector<uint32_t>& h : holes) holeVerts += h.size();
    }
    EXPECT_EQ(holedFaces, 1u) << "the hole did not survive the sew";
    EXPECT_EQ(holeRings, 1u);
    EXPECT_EQ(holeVerts, 8u) << "the reassembled ring has a different vertex count";

    EXPECT_EQ(out->faceCount(), src.faceCount());
    EXPECT_TRUE(out->checkIntegrity().ok) << out->checkIntegrity().reason;
    EXPECT_TRUE(out->checkGeometry().ok) << out->checkGeometry().reason;
}

// An assembled inner loop must be MARKED inner. If it were flagged outer it would read
// as a second outer boundary, and every consumer that subtracts holes (tessellation,
// area, mass properties) would silently treat the opening as material.
TEST(BRepFaceInnerLoopSew, AssembledInnerLoopIsFlaggedAsAHole)
{
    uint32_t f = kInvalid;
    const Body src = boxWithAHoledFace(f);
    ASSERT_NE(f, kInvalid);
    const std::optional<Body> out = roundTrip(src);
    ASSERT_TRUE(out.has_value());

    size_t inner = 0;
    for (uint32_t g = 0; g < static_cast<uint32_t>(out->faceCount()); ++g) {
        if (!out->face(g).alive) continue;
        for (uint32_t l : out->face(g).innerLoops) {
            ASSERT_LT(l, out->loopCount());
            EXPECT_FALSE(out->loop(l).outer) << "inner loop " << l << " is flagged outer";
            EXPECT_EQ(out->loop(l).face, g) << "inner loop " << l << " points at the wrong face";
            ++inner;
        }
    }
    EXPECT_EQ(inner, 1u);
}

// A hole ring's edges go through the same dedup and partnering as any other ring, so a
// second face sharing those edges partners with them rather than duplicating them. This
// is the property the curved sew needs: the other operand's ring is the same ring.
TEST(BRepFaceInnerLoopSew, HoleRingEdgesAreSharedNotDuplicated)
{
    // A square plate holed by a smaller square, plus a second face on the SAME hole ring
    // (a lid over the opening). The hole's four edges must be shared by both faces.
    const std::vector<Vec3> pts = {
        {-2.f, -2.f, 0.f}, {2.f, -2.f, 0.f}, {2.f, 2.f, 0.f}, {-2.f, 2.f, 0.f},   // outer 0..3
        {-1.f, -1.f, 0.f}, {1.f, -1.f, 0.f}, {1.f, 1.f, 0.f}, {-1.f, 1.f, 0.f},   // hole  4..7
    };
    Surface plane;
    plane.kind = SurfaceKind::Plane;
    plane.origin = {0.f, 0.f, 0.f};
    plane.normal = {0.f, 0.f, 1.f};
    plane.uAxis = {1.f, 0.f, 0.f};

    Body::FaceDef plate;
    plate.loop = {0u, 1u, 2u, 3u};              // CCW seen from +Z
    plate.innerLoops = {{7u, 6u, 5u, 4u}};      // CW — bounds a hole
    plate.surface = plane;

    Body::FaceDef lid;
    lid.loop = {4u, 5u, 6u, 7u};                // CCW: traverses the hole edges opposite
    lid.surface = plane;

    const std::optional<Body> b = Body::fromFaces(pts, {plate, lid});
    ASSERT_TRUE(b.has_value()) << "a plate with a hole plus a lid on that hole was rejected";

    // 8 distinct edges: 4 outer + 4 hole. If the hole ring had been duplicated instead of
    // shared there would be 12.
    size_t live = 0;
    for (uint32_t e = 0; e < static_cast<uint32_t>(b->edgeCount()); ++e)
        if (b->edge(e).alive) ++live;
    EXPECT_EQ(live, 8u) << "hole-ring edges were duplicated rather than shared";

    // The 4 hole edges carry two coedges each (plate + lid); the 4 outer ones carry one.
    std::vector<int> uses(b->edgeCount(), 0);
    for (uint32_t c = 0; c < static_cast<uint32_t>(b->coedgeCount()); ++c) {
        if (!b->coedge(c).alive) continue;
        if (b->coedge(c).edge < uses.size()) ++uses[b->coedge(c).edge];
    }
    int shared = 0, boundary = 0;
    for (size_t e = 0; e < uses.size(); ++e) {
        if (uses[e] == 2) ++shared;
        else if (uses[e] == 1) ++boundary;
    }
    EXPECT_EQ(shared, 4) << "the hole ring did not partner across the two faces";
    EXPECT_EQ(boundary, 4) << "the outer boundary should remain one-sided on an open plate";
    EXPECT_TRUE(b->checkIntegrity().ok) << b->checkIntegrity().reason;
}

// A degenerate hole ring is rejected rather than half-built.
TEST(BRepFaceInnerLoopSew, DegenerateInnerLoopIsRejected)
{
    const std::vector<Vec3> pts = {
        {-2.f, -2.f, 0.f}, {2.f, -2.f, 0.f}, {2.f, 2.f, 0.f}, {-2.f, 2.f, 0.f},
        {-1.f, -1.f, 0.f}, {1.f, -1.f, 0.f},
    };
    Surface plane;
    plane.kind = SurfaceKind::Plane;
    plane.normal = {0.f, 0.f, 1.f};
    plane.uAxis = {1.f, 0.f, 0.f};

    Body::FaceDef fd;
    fd.loop = {0u, 1u, 2u, 3u};
    fd.surface = plane;
    fd.innerLoops = {{4u, 5u}};  // only two vertices — not a ring
    EXPECT_FALSE(Body::fromFaces(pts, {fd}).has_value());

    Body::FaceDef oob = fd;
    oob.innerLoops = {{4u, 5u, 99u}};  // index past the point list
    EXPECT_FALSE(Body::fromFaces(pts, {oob}).has_value());
}

}  // namespace nexus::geometry::brep::testing
