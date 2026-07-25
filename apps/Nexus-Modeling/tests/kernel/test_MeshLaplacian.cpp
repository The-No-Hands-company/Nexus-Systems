#include <gtest/gtest.h>

#include <nexus/geometry/MeshLaplacian.h>
#include <nexus/geometry/Mesh.h>

#include <cmath>
#include <vector>

namespace {

using namespace nexus::geometry;

static Mesh makeSmallPlane()
{
    auto plane = primitives::makePlane(10.f, 10.f, 2, 2);
    (void)plane.topology().triangulate();
    return plane;
}

TEST(MeshLaplacian, SmoothPreservesVertexCount)
{
    auto plane = makeSmallPlane();
    ASSERT_TRUE(plane.isValid());
    size_t origCount = plane.attributes().vertexCount();
    SmoothOptions opts;
    opts.iterations = 3;
    opts.lambda = 0.3f;
    Mesh smoothed = MeshLaplacian::smooth(plane, opts);
    ASSERT_TRUE(smoothed.isValid());
    EXPECT_EQ(smoothed.attributes().vertexCount(), origCount);
}

TEST(MeshLaplacian, PlanarMeshStaysPlanarAfterSmoothing)
{
    auto plane = makeSmallPlane();
    ASSERT_TRUE(plane.isValid());
    SmoothOptions opts;
    opts.iterations = 5;
    opts.lambda = 0.5f;
    Mesh smoothed = MeshLaplacian::smooth(plane, opts);
    ASSERT_TRUE(smoothed.isValid());
    for (const auto& p : smoothed.attributes().positions()) {
        EXPECT_NEAR(p.y, 0.f, 1e-4f);
    }
}

TEST(MeshLaplacian, MeanCurvatureReturnsArrayOfCorrectSize)
{
    auto plane = makeSmallPlane();
    ASSERT_TRUE(plane.isValid());
    auto mc = MeshLaplacian::meanCurvature(plane);
    EXPECT_EQ(mc.size(), plane.attributes().vertexCount());
}

TEST(MeshLaplacian, MeanCurvatureNonNegative)
{
    auto plane = makeSmallPlane();
    ASSERT_TRUE(plane.isValid());
    auto mc = MeshLaplacian::meanCurvature(plane);
    ASSERT_FALSE(mc.empty());
    for (float v : mc) {
        EXPECT_GE(v, 0.f);
    }
}

// The cotangent Laplace-Beltrami operator vanishes on the interior of a planar mesh, so
// the discrete mean curvature there must be ~0 — a flat surface has zero curvature. The
// existing test only asserts >= 0, which any non-negative garbage would satisfy.
TEST(MeshLaplacian, MeanCurvatureIsZeroOnPlanarInterior)
{
    auto plane = primitives::makePlane(8.f, 8.f, 5, 5);  // XZ plane, y = 0
    (void)plane.topology().triangulate();
    ASSERT_TRUE(plane.isValid());

    auto mc = MeshLaplacian::meanCurvature(plane);
    const auto& pos = plane.attributes().positions();
    ASSERT_EQ(mc.size(), pos.size());

    for (size_t i = 0; i < mc.size(); ++i) {
        if (std::abs(pos[i].x) > 3.9f || std::abs(pos[i].z) > 3.9f) continue;  // skip boundary
        EXPECT_NEAR(mc[i], 0.f, 1e-4f) << "interior of a plane must have zero mean curvature";
    }
}

// A sphere of radius R has constant mean curvature; this implementation returns the sum of
// principal curvatures k1 + k2 = 2/R. Checking two radii pins both the value and its
// inverse-radius scaling, which exercises the cotangent weights AND the mixed Voronoi/
// obtuse area term together — a broken area or cotangent gives the wrong constant.
TEST(MeshLaplacian, MeanCurvatureMatchesSphereFormula)
{
    for (float R : {1.0f, 2.0f}) {
        auto sphere = primitives::makeSphere(R, 48, 48);
        (void)sphere.topology().triangulate();
        ASSERT_TRUE(sphere.isValid());

        auto mc = MeshLaplacian::meanCurvature(sphere);
        const auto& pos = sphere.attributes().positions();
        ASSERT_EQ(mc.size(), pos.size());

        // Interior band away from the pole fans, where the tessellation is regular.
        const float expected = 2.f / R;
        for (size_t i = 0; i < mc.size(); ++i) {
            if (std::abs(pos[i].y) > 0.7f * R) continue;
            EXPECT_NEAR(mc[i], expected, 0.02f * expected)
                << "R=" << R << " expected 2/R=" << expected;
        }
    }
}

// Laplacian smoothing is a low-pass filter: high-frequency displacement of a flat sheet must
// decay. Perturb the interior of a plane along its normal (y) with a deterministic zig-zag,
// then smooth; the interior normal-variance must collapse toward zero.
TEST(MeshLaplacian, SmoothingReducesNormalNoise)
{
    auto plane = primitives::makePlane(8.f, 8.f, 8, 8);  // XZ plane, y = 0
    (void)plane.topology().triangulate();
    ASSERT_TRUE(plane.isValid());

    auto pos = plane.attributes().positions();
    auto isInterior = [&](const auto& p) {
        return std::abs(p.x) < 3.9f && std::abs(p.z) < 3.9f;
    };

    // Deterministic +/-1 checkerboard displacement on interior vertices only.
    double before = 0.0;
    int nInterior = 0;
    for (auto& p : pos) {
        if (!isInterior(p)) continue;
        const int gx = static_cast<int>(std::lround(p.x));
        const int gz = static_cast<int>(std::lround(p.z));
        p.y = ((gx + gz) & 1) ? 1.f : -1.f;
        before += static_cast<double>(p.y) * p.y;
        ++nInterior;
    }
    plane.attributes().setPositions(std::move(pos));
    ASSERT_GT(nInterior, 0);
    before /= nInterior;

    SmoothOptions opts;
    opts.iterations = 40;
    opts.lambda = 0.5f;
    opts.fixBoundary = true;
    Mesh smoothed = MeshLaplacian::smooth(plane, opts);
    ASSERT_TRUE(smoothed.isValid());

    double after = 0.0;
    int m = 0;
    for (const auto& p : smoothed.attributes().positions()) {
        // classify by the original xz footprint (smoothing barely moves x/z on a flat sheet)
        if (std::abs(p.x) >= 3.9f || std::abs(p.z) >= 3.9f) continue;
        after += static_cast<double>(p.y) * p.y;
        ++m;
    }
    after /= m;

    EXPECT_LT(after, 0.05 * before) << "smoothing must attenuate normal noise (before="
                                     << before << " after=" << after << ")";
}

// With fixBoundary, boundary vertices must not move at all, even when interior smoothing
// pulls on them.
TEST(MeshLaplacian, SmoothingWithFixBoundaryKeepsBoundaryExact)
{
    auto plane = primitives::makePlane(8.f, 8.f, 6, 6);
    (void)plane.topology().triangulate();
    ASSERT_TRUE(plane.isValid());

    // Displace the interior so smoothing has something to do.
    auto pos = plane.attributes().positions();
    std::vector<size_t> boundary;
    for (size_t i = 0; i < pos.size(); ++i) {
        if (std::abs(pos[i].x) >= 3.9f || std::abs(pos[i].z) >= 3.9f) boundary.push_back(i);
        else pos[i].y += 0.5f;
    }
    const auto original = pos;  // capture boundary positions post-setup
    plane.attributes().setPositions(std::move(pos));
    ASSERT_FALSE(boundary.empty());

    SmoothOptions opts;
    opts.iterations = 15;
    opts.lambda = 0.5f;
    opts.fixBoundary = true;
    Mesh smoothed = MeshLaplacian::smooth(plane, opts);
    ASSERT_TRUE(smoothed.isValid());

    const auto& out = smoothed.attributes().positions();
    for (size_t bi : boundary) {
        EXPECT_FLOAT_EQ(out[bi].x, original[bi].x);
        EXPECT_FLOAT_EQ(out[bi].y, original[bi].y);
        EXPECT_FLOAT_EQ(out[bi].z, original[bi].z);
    }
}

} // namespace
