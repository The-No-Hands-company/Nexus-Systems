#include <gtest/gtest.h>
#include <cmath>
#include <vector>

#include "nexus/utility/graphics/convex_hull3d.h"
#include "nexus/utility/graphics/signed_distance_field.h"
#include "nexus/utility/graphics/surface_curvature.h"

using namespace nexus::utility::graphics;

// ── ConvexHull3D ────────────────────────────────────────────────────────────

TEST(ConvexHull3DAdvTest, CubeHullVolume) {
    ConvexHull3D ch;
    std::vector<Vector3> pts;
    for (int x = 0; x <= 1; ++x)
        for (int y = 0; y <= 1; ++y)
            for (int z = 0; z <= 1; ++z)
                pts.push_back({static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)});
    ch.computeHull(pts);
    EXPECT_NEAR(ch.getVolume(), 1.0f, 0.1f);
    EXPECT_GT(ch.getSurfaceArea(), 0.0f);
}

TEST(ConvexHull3DAdvTest, PointInside) {
    ConvexHull3D ch;
    std::vector<Vector3> cube = {{0,0,0},{2,0,0},{0,2,0},{0,0,2},{2,2,0},{2,0,2},{0,2,2},{2,2,2}};
    ch.computeHull(cube);
    EXPECT_TRUE(ch.isInside({1,1,1}));
    EXPECT_FALSE(ch.isInside({3,1,1}));
}

// ── SignedDistanceField ─────────────────────────────────────────────────────

TEST(SignedDistanceFieldTest, SphereSDFValue) {
    auto fn = [](const Vector3& p) -> float {
        float dx = p.x, dy = p.y, dz = p.z;
        return std::sqrt(dx*dx + dy*dy + dz*dz) - 1.0f;
    };
    auto sdf = SignedDistanceField::fromFunction(fn, {-2,-2,-2}, {2,2,2}, 32);
    float val = sdf.evaluate({2,0,0});
    EXPECT_NEAR(val, 1.0f, 0.2f);
}

TEST(SignedDistanceFieldTest, Gradient) {
    auto fn = [](const Vector3& p) -> float {
        return std::sqrt(p.x*p.x + p.y*p.y + p.z*p.z) - 1.0f;
    };
    auto sdf = SignedDistanceField::fromFunction(fn, {-2,-2,-2}, {2,2,2}, 32);
    auto grad = sdf.gradient({2,0,0});
    EXPECT_GT(grad.x, 0.0f);
}

// ── SurfaceCurvature ────────────────────────────────────────────────────────

TEST(SurfaceCurvatureTest, UnitSphereMeanCurvature) {
    // Build sphere mesh
    std::vector<Vector3> verts;
    std::vector<unsigned> idx;
    const int rings = 8, segs = 16;
    for (int i = 0; i <= rings; ++i) {
        float phi = static_cast<float>(i) * 3.14159f / rings;
        for (int j = 0; j < segs; ++j) {
            float theta = static_cast<float>(j) * 2.0f * 3.14159f / segs;
            verts.push_back({sinf(phi)*cosf(theta), cosf(phi), sinf(phi)*sinf(theta)});
        }
    }
    for (int i = 0; i < rings; ++i)
        for (int j = 0; j < segs; ++j) {
            unsigned a = static_cast<unsigned>(i*segs+j), b = static_cast<unsigned>(i*segs+(j+1)%segs);
            unsigned c = static_cast<unsigned>((i+1)*segs+j), d = static_cast<unsigned>((i+1)*segs+(j+1)%segs);
            idx.insert(idx.end(), {a,b,c, b,d,c});
        }
    // Curvature of unit sphere should be ~1.0
    float mean = SurfaceCurvature::computeMeanCurvature(verts, idx, static_cast<unsigned>(segs * 4));
    EXPECT_NEAR(mean, 1.0f, 0.3f);
}
