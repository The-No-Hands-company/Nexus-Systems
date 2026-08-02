#include <gtest/gtest.h>

#include <nexus/geometry/FeatureLineExtractor.h>
#include <nexus/geometry/Mesh.h>

#include <cmath>
#include <vector>

namespace {

using namespace nexus::geometry;
using namespace nexus::geometry::primitives;

TEST(FeatureLineExtractor, FlatPlaneProducesNoLines)
{
    Mesh plane = makePlane(4.0f, 4.0f, 8u, 8u);

    FeatureLineExtractorOptions opts;
    opts.dihedralAngleThresholdDegrees = 1.0f;

    const auto result = FeatureLineExtractor::extract(plane, opts);

    EXPECT_EQ(result.size(), 0u);
}

// A 2x2x2 box has twelve sharp edges of length 2, so the feature lines total EXACTLY 24
// and every point sits on an edge of the cube (two of |x|,|y|,|z| equal 1). The previous
// assertion was `size() > 0` — one polyline of any shape anywhere would satisfy it.
TEST(FeatureLineExtractor, BoxFeatureLinesAreTheTwelveCubeEdges)
{
    Mesh box = makeBox(2.0f, 2.0f, 2.0f);

    // The cube's dihedral angle is 90 degrees, so every threshold below it must give the
    // same answer — a result that drifts with the threshold is keying off something else.
    for (float threshold : {5.0f, 30.0f, 60.0f, 89.0f}) {
        FeatureLineExtractorOptions opts;
        opts.dihedralAngleThresholdDegrees = threshold;
        const auto result = FeatureLineExtractor::extract(box, opts);
        ASSERT_FALSE(result.empty()) << "threshold " << threshold;

        double total = 0.0;
        for (const auto& pl : result) {
            EXPECT_GE(pl.points.size(), 2u);
            for (size_t i = 1; i < pl.points.size(); ++i) {
                const auto& a = pl.points[i - 1];
                const auto& b = pl.points[i];
                const double dx = b.x - a.x, dy = b.y - a.y, dz = b.z - a.z;
                total += std::sqrt(dx * dx + dy * dy + dz * dz);
            }
            for (const auto& p : pl.points) {
                int onFace = 0;
                for (float c : {p.x, p.y, p.z}) {
                    if (std::abs(std::abs(c) - 1.0f) < 1e-4f) ++onFace;
                }
                EXPECT_GE(onFace, 2) << "a feature point is not on an edge of the cube";
            }
        }
        EXPECT_NEAR(total, 24.0, 1e-3) << "12 edges of length 2, threshold " << threshold;
    }
}

// The name said "HasFeatures" and the assertion was `size() >= 0` — unsigned, so it held
// just as well for no features at all, which is the opposite of the claim.
TEST(FeatureLineExtractor, SphereWithLowThresholdHasFeatures)
{
    Mesh sphere = makeSphere(1.0f, 32u, 32u);

    FeatureLineExtractorOptions opts;
    opts.dihedralAngleThresholdDegrees = 0.1f;
    const auto result = FeatureLineExtractor::extract(sphere, opts);

    EXPECT_GT(result.size(), 0u) << "every facet edge of a 32x32 sphere exceeds 0.1 degrees";
    for (const auto& pl : result) EXPECT_GE(pl.points.size(), 2u);

    // A high threshold on the same mesh must find nothing, or the threshold is ignored.
    FeatureLineExtractorOptions coarse;
    coarse.dihedralAngleThresholdDegrees = 90.0f;
    EXPECT_TRUE(FeatureLineExtractor::extract(sphere, coarse).empty())
        << "a smooth sphere has no 90-degree edges";
}

TEST(FeatureLineExtractor, HighThresholdProducesNoLines)
{
    Mesh box = makeBox(2.0f, 2.0f, 2.0f);

    FeatureLineExtractorOptions opts;
    opts.dihedralAngleThresholdDegrees = 180.0f;

    const auto result = FeatureLineExtractor::extract(box, opts);

    EXPECT_EQ(result.size(), 0u);
}

TEST(FeatureLineExtractor, InvalidMeshReturnsEmpty)
{
    Mesh empty;

    const auto result = FeatureLineExtractor::extract(empty);

    EXPECT_EQ(result.size(), 0u);
}

} // namespace
