#include <gtest/gtest.h>

#include <nexus/geometry/TransformOrientation.h>

using namespace nexus::geometry;

TEST(TransformOrientation, FromGlobalIsIdentity) {
    auto frame = TransformOrientation::fromGlobal({1, 2, 3});
    EXPECT_NEAR(frame.origin.x, 1.0f, 0.001f);
    EXPECT_NEAR(frame.axisX.x, 1.0f, 0.001f);
    EXPECT_NEAR(frame.axisY.y, 1.0f, 0.001f);
    EXPECT_NEAR(frame.axisZ.z, 1.0f, 0.001f);
}

TEST(TransformOrientation, FromFaceNormalIsOrthonormal) {
    auto frame = TransformOrientation::fromFace({0, 0, 1}, {0, 0, 0});
    EXPECT_TRUE(TransformOrientation::isOrthonormal(frame));
}

TEST(TransformOrientation, FromEdgeIsOrthonormal) {
    auto frame = TransformOrientation::fromEdge({0, 0, 0}, {1, 0, 0});
    EXPECT_TRUE(TransformOrientation::isOrthonormal(frame));
}

TEST(TransformOrientation, FromViewIsOrthonormal) {
    auto frame = TransformOrientation::fromView({0, 0, 10}, {0, 0, 0}, {0, 1, 0});
    EXPECT_TRUE(TransformOrientation::isOrthonormal(frame));
}

TEST(TransformOrientation, FromCursorIsIdentityAtPosition) {
    auto frame = TransformOrientation::fromCursor({5, 5, 5});
    EXPECT_NEAR(frame.origin.x, 5.0f, 0.001f);
    EXPECT_TRUE(TransformOrientation::isOrthonormal(frame));
}

TEST(TransformOrientation, NonOrthonormalFrameIsDetected) {
    TransformFrame bad;
    bad.origin = {};
    bad.axisX = {1, 0, 0};
    bad.axisY = {1, 0, 0}; // collinear with X — not orthonormal
    bad.axisZ = {0, 0, 1};
    EXPECT_FALSE(TransformOrientation::isOrthonormal(bad));
}
