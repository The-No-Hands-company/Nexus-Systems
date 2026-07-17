// Foundation — analytic B-rep IMPRINT (the boolean's imprint step). Splitting a
// planar face along an analytic intersection curve, with the resulting shared
// edge lying exactly on that curve. Proven χ-neutral, integrity- and
// geometry-clean, on-both-surfaces, and composable with mergeFaces (its inverse
// direction). Line imprint (plane∩plane) is the case landed here.

#include <nexus/geometry/AnalyticBRep.h>
#include <nexus/geometry/BRepSurfaceIntersect.h>
#include <nexus/geometry/RobustPredicates.h>

#include <gtest/gtest.h>

#include <cmath>

namespace nexus::geometry::brep::testing {

using nexus::render::Vec3;

namespace {
// The +Z face of a box centred at the origin (all four verts at z = +halfDepth).
uint32_t topFace(const Body& b, float z)
{
    for (uint32_t f = 0; f < b.faceCount(); ++f) {
        const auto vs = b.faceVertices(f);
        if (vs.empty()) continue;
        bool all = true;
        for (uint32_t v : vs)
            if (std::abs(b.vertex(v).point.z - z) > 1e-4f) { all = false; break; }
        if (all) return f;
    }
    return kInvalid;
}

Surface plane(Vec3 o, Vec3 n) { Surface s; s.kind = SurfaceKind::Plane; s.origin = o; s.normal = n; return s; }
}  // namespace

TEST(BRepImprint, LineSplitsPlanarFaceEdgeToEdge)
{
    Body box = makeBox(2.f, 2.f, 2.f);  // [-1,1]^3
    ASSERT_TRUE(box.checkIntegrity().ok);
    ASSERT_TRUE(box.checkGeometry().ok);

    const uint32_t f = topFace(box, 1.f);
    ASSERT_NE(f, kInvalid);

    // plane z=+1 (the top face) ∩ plane x=0 → the Line x=0 in the z=1 plane.
    const auto ssi = intersectSurfaces(plane({0, 0, 1}, {0, 0, 1}), plane({0, 0, 0}, {1, 0, 0}));
    ASSERT_EQ(ssi.kind, SurfaceIntersectionKind::Line);

    const size_t V0 = box.vertexCount(), E0 = box.edgeCount(), F0 = box.faceCount();
    const uint32_t nf = box.imprintCurve(f, ssi.curve);
    ASSERT_NE(nf, kInvalid);

    // χ-neutral: ΔV=+2 (pierce verts), ΔE=+3 (two splits + the cut), ΔF=+1.
    EXPECT_EQ(box.vertexCount(), V0 + 2);
    EXPECT_EQ(box.edgeCount(), E0 + 3);
    EXPECT_EQ(box.faceCount(), F0 + 1);

    const auto ig = box.checkIntegrity();
    EXPECT_TRUE(ig.ok) << ig.reason;
    EXPECT_EQ(ig.euler, 2);                 // still a closed genus-0 solid
    EXPECT_EQ(ig.boundaryEdges, 0u);        // still watertight
    EXPECT_TRUE(box.checkGeometry().ok) << box.checkGeometry().reason;
    EXPECT_TRUE(box.isClosed());
}

TEST(BRepImprint, CutEdgeLiesOnBothSurfaces)
{
    Body box = makeBox(2.f, 2.f, 2.f);
    const uint32_t f = topFace(box, 1.f);
    const Surface top = plane({0, 0, 1}, {0, 0, 1});
    const Surface cut = plane({0, 0, 0}, {1, 0, 0});
    const auto ssi = intersectSurfaces(top, cut);
    ASSERT_NE(box.imprintCurve(f, ssi.curve), kInvalid);

    // The new cut edge (a Line at x=0, z=1 spanning y) must lie on BOTH planes.
    int found = -1;
    float maxd = 0.f;
    for (uint32_t e = 0; e < box.edgeCount(); ++e) {
        const auto& ed = box.edge(e);
        if (!ed.alive) continue;
        const auto& cu = box.curve(ed.curve);
        if (cu.kind != CurveKind::Line) continue;
        const Vec3 p0 = box.vertex(ed.v0).point, p1 = box.vertex(ed.v1).point;
        if (std::abs(p0.x) < 1e-4f && std::abs(p1.x) < 1e-4f &&
            std::abs(p0.z - 1.f) < 1e-4f && std::abs(p1.z - 1.f) < 1e-4f &&
            std::abs(p0.y - p1.y) > 0.5f) {
            found = static_cast<int>(e);
            for (int k = 0; k <= 12; ++k) {
                const float t = ed.t0 + (ed.t1 - ed.t0) * static_cast<float>(k) / 12.f;
                const Vec3 p = cu.eval(t);
                maxd = std::max(maxd, std::max(std::abs(surfaceDistance(top, p)),
                                               std::abs(surfaceDistance(cut, p))));
            }
        }
    }
    ASSERT_GE(found, 0);
    EXPECT_LT(maxd, 1e-4f);
}

TEST(BRepImprint, CurveMissingFaceReturnsInvalid)
{
    Body box = makeBox(2.f, 2.f, 2.f);
    const uint32_t f = topFace(box, 1.f);
    const size_t E0 = box.edgeCount(), F0 = box.faceCount();
    // plane x=5 does not cross the [-1,1] top face → its intersection Line misses.
    const auto ssi = intersectSurfaces(plane({0, 0, 1}, {0, 0, 1}), plane({5, 0, 0}, {1, 0, 0}));
    ASSERT_EQ(ssi.kind, SurfaceIntersectionKind::Line);
    EXPECT_EQ(box.imprintCurve(f, ssi.curve), kInvalid);
    // Body is left unchanged and still valid.
    EXPECT_EQ(box.edgeCount(), E0);
    EXPECT_EQ(box.faceCount(), F0);
    EXPECT_TRUE(box.checkIntegrity().ok);
}

TEST(BRepImprint, ComposesWithMergeFacesInverse)
{
    Body box = makeBox(2.f, 2.f, 2.f);
    const uint32_t f = topFace(box, 1.f);
    const auto ssi = intersectSurfaces(plane({0, 0, 1}, {0, 0, 1}), plane({0, 0, 0}, {1, 0, 0}));
    const size_t F0 = box.faceCount();
    ASSERT_NE(box.imprintCurve(f, ssi.curve), kInvalid);
    ASSERT_EQ(box.faceCount(), F0 + 1);

    // Locate the cut edge and merge the two faces back (kill-edge-face).
    uint32_t cutEdge = kInvalid;
    for (uint32_t e = 0; e < box.edgeCount(); ++e) {
        const auto& ed = box.edge(e);
        if (!ed.alive) continue;
        const Vec3 p0 = box.vertex(ed.v0).point, p1 = box.vertex(ed.v1).point;
        if (std::abs(p0.x) < 1e-4f && std::abs(p1.x) < 1e-4f &&
            std::abs(p0.z - 1.f) < 1e-4f && std::abs(p1.z - 1.f) < 1e-4f &&
            std::abs(p0.y - p1.y) > 0.5f) { cutEdge = e; break; }
    }
    ASSERT_NE(cutEdge, kInvalid);
    ASSERT_TRUE(box.mergeFaces(cutEdge));
    const auto ig = box.checkIntegrity();
    EXPECT_TRUE(ig.ok) << ig.reason;
    EXPECT_EQ(ig.faces, static_cast<uint32_t>(F0));  // face count restored
    EXPECT_EQ(ig.euler, 2);
    EXPECT_TRUE(box.checkGeometry().ok);
}

// ──────────── Exact in-plane straddle (imprint crossing decision) ─────────────

// The exact orient3D-based in-plane straddle now used by segmentLineCrossing
// matches an exact int64 2D ground truth on every case, while the old float
// closest-approach `s` + band path mis-decides near-endpoint crossings at large
// coordinates (a crossing at s < 0.02 is rejected though the edge truly crosses).
TEST(BRepImprint, ExactInPlaneStraddleBeatsNaiveFloat)
{
    using nexus::geometry::RobustPredicates;
    const Vec3 O{0, 0, 0};
    const float inv2 = 0.70710678f;
    const Vec3 D{inv2, inv2, 0.f};  // imprint line direction (the line y=x in z=0)
    const Vec3 nrm{0, 0, 1};        // face-plane normal

    // Exact in-plane side via orient3D(O, O+D, P, O+n) — what the code computes.
    auto exactSide = [&](const Vec3& P) {
        return RobustPredicates::orient3D(O, {O.x + D.x, O.y + D.y, O.z + D.z}, P,
                                          {O.x + nrm.x, O.y + nrm.y, O.z + nrm.z});
    };
    // The OLD float path: closest-approach fraction s + interior band + coplanar.
    auto vcross = [](const Vec3& u, const Vec3& v) {
        return Vec3{u.y * v.z - u.z * v.y, u.z * v.x - u.x * v.z, u.x * v.y - u.y * v.x};
    };
    auto vdot = [](const Vec3& u, const Vec3& v) { return u.x * v.x + u.y * v.y + u.z * v.z; };
    auto naiveCross = [&](const Vec3& A, const Vec3& B) {
        const Vec3 E{B.x - A.x, B.y - A.y, B.z - A.z};
        const Vec3 ExD = vcross(E, D);
        const float denom = vdot(ExD, ExD);
        if (denom < 1e-20f) return false;
        const Vec3 OmA{O.x - A.x, O.y - A.y, O.z - A.z};
        const float s = vdot(vcross(OmA, D), ExD) / denom;
        if (s <= 0.02f || s >= 0.98f) return false;
        const Vec3 P{A.x + E.x * s, A.y + E.y * s, A.z + E.z * s};
        const Vec3 w{P.x - O.x, P.y - O.y, P.z - O.z};
        const float wd = vdot(w, D);
        const Vec3 perp{w.x - D.x * wd, w.y - D.y * wd, w.z - D.z * wd};
        return std::sqrt(vdot(perp, perp)) <= 1e-3f;
    };

    const long long Qb = 2000000;          // large float32-exact base
    const long long Boff = 1000000;        // B far on the − side of y=x
    int definite = 0, exactWrong = 0, naiveWrong = 0;
    for (long long a : {-40000LL, -500LL, -3LL, 3LL, 50LL, 800LL, 30000LL, 300000LL}) {
        const Vec3 A{static_cast<float>(Qb + a), static_cast<float>(Qb), 0.f};
        const Vec3 B{static_cast<float>(Qb), static_cast<float>(Qb + Boff), 0.f};
        // Ground truth (exact int64): A,B straddle y=x iff (Ax−Ay) and (Bx−By)
        // have opposite signs. Bx−By = −Boff (< 0), so straddle ⇔ a > 0.
        const bool gt = a > 0;
        const double sa = exactSide(A), sb = exactSide(B);
        const bool exact = (sa != 0.0 && sb != 0.0) && ((sa > 0.0) != (sb > 0.0));
        const bool naive = naiveCross(A, B);
        ++definite;
        if (exact != gt) ++exactWrong;
        if (naive != gt) ++naiveWrong;
    }
    EXPECT_GT(definite, 5);
    EXPECT_EQ(exactWrong, 0);  // orient3D straddle is exact on every case
    EXPECT_GT(naiveWrong, 0);  // the old float s+band mis-decides near-endpoint crossings
}

// The wired path: a Line imprint on a LARGE-coordinate planar face stays exact —
// χ-neutral and both validators clean — where the float parameter would be shaky.
TEST(BRepImprint, LargeCoordinateLineImprintIsClean)
{
    Body box = makeBox(2000000.f, 2000000.f, 2000000.f);  // [-1e6, 1e6]^3
    ASSERT_TRUE(box.checkIntegrity().ok);

    const uint32_t f = topFace(box, 1000000.f);
    ASSERT_NE(f, kInvalid);
    // top plane (z = +1e6) ∩ plane x=0 → the Line x=0 in that plane.
    const auto ssi = intersectSurfaces(plane({0, 0, 1000000}, {0, 0, 1}), plane({0, 0, 0}, {1, 0, 0}));
    ASSERT_EQ(ssi.kind, SurfaceIntersectionKind::Line);

    const size_t V0 = box.vertexCount(), E0 = box.edgeCount(), F0 = box.faceCount();
    const uint32_t nf = box.imprintCurve(f, ssi.curve);
    ASSERT_NE(nf, kInvalid);
    EXPECT_EQ(box.vertexCount(), V0 + 2);
    EXPECT_EQ(box.edgeCount(), E0 + 3);
    EXPECT_EQ(box.faceCount(), F0 + 1);
    const auto ig = box.checkIntegrity();
    EXPECT_TRUE(ig.ok) << ig.reason;
    EXPECT_EQ(ig.euler, 2);
    EXPECT_TRUE(box.checkGeometry().ok);
}

}  // namespace nexus::geometry::brep::testing
