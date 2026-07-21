// Foundation — NEAR-TANGENT torture on the analytic B-rep exactness path. Grazing /
// near-tangent / near-coincident configurations are where an exactness or robustness
// bug hides best. Two guarantees are pinned here:
//   • intersectSurfaces returns a curve whose sampled points lie on BOTH surfaces
//     (or a clean None/Point) — never a bogus tangent curve, never non-finite.
//   • booleanToBody TERMINATES in bounded time and is watertight-or-empty +
//     deterministic — even on a faceted sphere grazing a box face by ~1e-4, which
//     used to imprint an O(n²) line arrangement and effectively HANG. The imprint
//     now bails its face budget on such a degeneracy and the Boolean returns a clean
//     empty Body instead of grinding forever.

#include <nexus/geometry/AnalyticBRep.h>
#include <nexus/geometry/BRepBoolean.h>
#include <nexus/geometry/BRepSurfaceIntersect.h>
#include <nexus/geometry/Tolerance.h>

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

namespace nexus::geometry::brep::testing {

using nexus::render::Vec3;

namespace {
Surface plane(Vec3 o, Vec3 n) { return {SurfaceKind::Plane, o, n, {1, 0, 0}, 0.f, kInvalid}; }
Surface sphereS(Vec3 c, float r) { return {SurfaceKind::Sphere, c, {0, 0, 1}, {1, 0, 0}, r, kInvalid}; }
Surface cylS(Vec3 o, Vec3 ax, float r) { return {SurfaceKind::Cylinder, o, ax, {1, 0, 0}, r, kInvalid}; }

// Every sampled point of a returned Line/Circle must lie on BOTH surfaces.
void expectCurveOnBothOrClean(const Surface& a, const Surface& b)
{
    const SurfaceIntersection r = intersectSurfaces(a, b);
    const std::vector<const Curve*> cs =
        (r.kind == SurfaceIntersectionKind::TwoLines) ? std::vector<const Curve*>{&r.curve, &r.curve2}
        : (r.kind == SurfaceIntersectionKind::Line || r.kind == SurfaceIntersectionKind::Circle)
            ? std::vector<const Curve*>{&r.curve}
            : std::vector<const Curve*>{};
    for (const Curve* c : cs) {
        for (int i = 0; i <= 24; ++i) {
            const float t = (c->kind == CurveKind::Circle) ? (6.2831853f * static_cast<float>(i) / 24.f)
                                                           : (-2.f + 4.f * static_cast<float>(i) / 24.f);
            const Vec3 p = c->eval(t);
            ASSERT_TRUE(isFinite(p));
            EXPECT_LT(std::abs(surfaceDistance(a, p)), 1e-3f);
            EXPECT_LT(std::abs(surfaceDistance(b, p)), 1e-3f);
        }
    }
    if (r.kind == SurfaceIntersectionKind::Point) EXPECT_TRUE(isFinite(r.point));
}
}  // namespace

// Surface–surface intersection stays exact-on-both (or a clean miss) at near-tangency.
TEST(BRepNearTangentTorture, SurfaceIntersectionCurveLiesOnBothSurfaces)
{
    for (float eps : {1e-2f, 1e-3f, 1e-4f, 1e-5f, 1e-6f, 0.f}) {
        for (int s : {-1, 1}) {
            const float d = 1.f + static_cast<float>(s) * eps;
            expectCurveOnBothOrClean(plane({0, 0, d}, {0, 0, 1}), sphereS({0, 0, 0}, 1.f));
            expectCurveOnBothOrClean(sphereS({0, 0, 0}, 1.f), sphereS({2.f + static_cast<float>(s) * eps, 0, 0}, 1.f));
            expectCurveOnBothOrClean(sphereS({0, 0, 0}, 1.f), sphereS({static_cast<float>(s) * eps, 0, 0}, 1.f));
        }
    }
    for (float ang : {0.f, 1e-4f, 1e-3f, 1e-2f, 0.1f}) {
        expectCurveOnBothOrClean(plane({0, 0, 0.3f}, {std::sin(ang), 0.f, std::cos(ang)}),
                                 cylS({0, 0, 0}, {0, 0, 1}, 1.f));
    }
}

// The Boolean TERMINATES (bounded time) and is watertight-or-empty + deterministic
// across grazing faceted / box configs — the case that used to hang.
TEST(BRepNearTangentTorture, BooleanBoundedWatertightOrEmptyUnderGrazing)
{
    auto watertightOrEmpty = [](const Body& r) {
        const auto ig = r.checkIntegrity();
        return ig.ok && (r.faceCount() == 0 || (r.isClosed() && ig.boundaryEdges == 0u));
    };
    for (float eps : {0.f, 1e-4f, 1e-3f, -1e-4f, -1e-3f}) {
        const Body box = makeBox(2.f, 2.f, 2.f);
        Body fs = makeFacetedSphere(1.f, 6, 8);
        fs.translate({2.f + eps, 0.f, 0.f});  // grazes the +x face; -eps overlaps by |eps|
        Body b2 = makeBox(2.f, 2.f, 2.f);
        b2.translate({2.f + eps, 0.f, 0.f});
        for (BooleanOp op : {BooleanOp::Union, BooleanOp::Intersection, BooleanOp::Difference}) {
            const Body r1 = booleanToBody(box, fs, op);
            EXPECT_TRUE(watertightOrEmpty(r1)) << "box?sphere eps=" << eps << " op=" << static_cast<int>(op);
            EXPECT_EQ(r1.serialize(), booleanToBody(box, fs, op).serialize());  // deterministic
            EXPECT_TRUE(watertightOrEmpty(booleanToBody(box, b2, op))) << "box?box eps=" << eps;
        }
    }
}

}  // namespace nexus::geometry::brep::testing
