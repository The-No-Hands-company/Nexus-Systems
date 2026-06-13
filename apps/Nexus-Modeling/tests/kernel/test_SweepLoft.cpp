#include <gtest/gtest.h>

#include <nexus/geometry/SweepLoft.h>
#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/NurbsSurface.h>

#include <cmath>
#include <vector>

namespace {

using namespace nexus::geometry;

static NurbsSurface makeCurveLine(Vec3 a, Vec3 b) {
    std::vector<float> kU = {0.f, 0.f, 1.f, 1.f};
    std::vector<float> kV = {0.f, 0.f, 1.f, 1.f};
    std::vector<Vec3> ctl = {a, a, b, b};
    return NurbsSurface(1, 1, kU, kV, ctl, 2, 2);
}

TEST(SweepLoft, SweepLineAlongVectorProducesPlane) {
    NurbsSurface profile = makeCurveLine({-1, 0, 0}, {1, 0, 0});
    NurbsSurface path = makeCurveLine({0, 0, 0}, {0, 0, 4});

    SweepDesc desc;
    desc.crossSectionCount = 2;

    Mesh mesh;
    NurbsSurface outSurface;
    SweepLoftDiagnostic diag = SweepLoftOperation::sweep(profile, path, desc, outSurface, &mesh);

    EXPECT_EQ(diag, SweepLoftDiagnostic::Success);
    EXPECT_GT(mesh.attributes().vertexCount(), 0u);
    EXPECT_GT(mesh.topology().faceCount(), 0u);

    // crossSectionCount=2, profSamples=2 (line has 2 CPs)
    EXPECT_EQ(mesh.attributes().vertexCount(), 4u);
    EXPECT_EQ(mesh.topology().faceCount(), 2u);

    for (const auto& v : mesh.attributes().positions()) {
        EXPECT_NEAR(v.y, 0.f, 1e-5f);
    }
}

TEST(SweepLoft, SweepVertexFaceCounts) {
    NurbsSurface profile = makeCurveLine({-1, 0, 0}, {1, 0, 0});
    NurbsSurface path = makeCurveLine({0, 0, 0}, {0, 0, 4});

    SweepDesc desc;

    Mesh mesh;
    NurbsSurface outSurface;
    SweepLoftDiagnostic diag = SweepLoftOperation::sweep(profile, path, desc, outSurface, &mesh);

    EXPECT_EQ(diag, SweepLoftDiagnostic::Success);

    // Default crossSectionCount=20, profSamples=2
    uint32_t expectedVerts = desc.crossSectionCount * 2;
    uint32_t expectedFaces = (desc.crossSectionCount - 1) * 1 * 2;
    EXPECT_EQ(mesh.attributes().vertexCount(), static_cast<size_t>(expectedVerts));
    EXPECT_EQ(mesh.topology().faceCount(), static_cast<size_t>(expectedFaces));
}

TEST(SweepLoft, SweepVertexFaceCountsCustomCrossSections) {
    NurbsSurface profile = makeCurveLine({-1, 0, 0}, {1, 0, 0});
    NurbsSurface path = makeCurveLine({0, 0, 0}, {0, 0, 4});

    for (uint32_t cs = 2; cs <= 10; ++cs) {
        SweepDesc desc;
        desc.crossSectionCount = cs;

        Mesh mesh;
        NurbsSurface outSurface;
        SweepLoftDiagnostic diag = SweepLoftOperation::sweep(profile, path, desc, outSurface, &mesh);

        EXPECT_EQ(diag, SweepLoftDiagnostic::Success);

        uint32_t expectedVerts = cs * 2;
        uint32_t expectedFaces = (cs - 1) * 1 * 2;
        EXPECT_EQ(mesh.attributes().vertexCount(), static_cast<size_t>(expectedVerts));
        EXPECT_EQ(mesh.topology().faceCount(), static_cast<size_t>(expectedFaces));
    }
}

TEST(SweepLoft, LoftBetweenTwoParallelLinesProducesPlane) {
    NurbsSurface profile = makeCurveLine({-2, 0, 0}, {2, 0, 0});
    NurbsSurface path = makeCurveLine({0, 0, -3}, {0, 0, 3});

    SweepDesc desc;
    desc.crossSectionCount = 3;

    Mesh mesh;
    NurbsSurface outSurface;
    SweepLoftDiagnostic diag = SweepLoftOperation::sweep(profile, path, desc, outSurface, &mesh);

    EXPECT_EQ(diag, SweepLoftDiagnostic::Success);

    EXPECT_EQ(mesh.attributes().vertexCount(), 6u);
    EXPECT_EQ(mesh.topology().faceCount(), 4u);

    for (const auto& v : mesh.attributes().positions()) {
        EXPECT_NEAR(v.y, 0.f, 1e-5f);
    }
}

TEST(SweepLoft, LoftVertexFaceCounts) {
    NurbsSurface profile = makeCurveLine({-5, 0, 0}, {5, 0, 0});
    NurbsSurface path = makeCurveLine({0, 0, -10}, {0, 0, 10});

    SweepDesc desc;
    desc.crossSectionCount = 10;

    Mesh mesh;
    NurbsSurface outSurface;
    SweepLoftDiagnostic diag = SweepLoftOperation::sweep(profile, path, desc, outSurface, &mesh);

    EXPECT_EQ(diag, SweepLoftDiagnostic::Success);

    uint32_t expectedVerts = desc.crossSectionCount * 2;
    uint32_t expectedFaces = (desc.crossSectionCount - 1) * 1 * 2;
    EXPECT_EQ(mesh.attributes().vertexCount(), static_cast<size_t>(expectedVerts));
    EXPECT_EQ(mesh.topology().faceCount(), static_cast<size_t>(expectedFaces));
}

TEST(SweepLoft, InvalidProfileReturnsError) {
    NurbsSurface invalidProfile;
    NurbsSurface path = makeCurveLine({0, 0, 0}, {0, 0, 4});

    SweepDesc desc;
    Mesh mesh;
    NurbsSurface outSurface;
    SweepLoftDiagnostic diag = SweepLoftOperation::sweep(invalidProfile, path, desc, outSurface, &mesh);

    EXPECT_EQ(diag, SweepLoftDiagnostic::InvalidProfileCurve);
    EXPECT_EQ(mesh.attributes().vertexCount(), 0u);
    EXPECT_EQ(mesh.topology().faceCount(), 0u);
}

TEST(SweepLoft, InvalidPathReturnsError) {
    NurbsSurface profile = makeCurveLine({-1, 0, 0}, {1, 0, 0});
    NurbsSurface invalidPath;

    SweepDesc desc;
    Mesh mesh;
    NurbsSurface outSurface;
    SweepLoftDiagnostic diag = SweepLoftOperation::sweep(profile, invalidPath, desc, outSurface, &mesh);

    EXPECT_EQ(diag, SweepLoftDiagnostic::InvalidPathCurve);
    EXPECT_EQ(mesh.attributes().vertexCount(), 0u);
    EXPECT_EQ(mesh.topology().faceCount(), 0u);
}

TEST(SweepLoft, PathTooShortReturnsError) {
    NurbsSurface profile = makeCurveLine({-1, 0, 0}, {1, 0, 0});

    std::vector<float> kU = {5.f, 5.f, 5.f, 5.f};
    std::vector<float> kV = {0.f, 0.f, 1.f, 1.f};
    std::vector<Vec3> ctl = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}};
    NurbsSurface zeroPath(1, 1, kU, kV, ctl, 2, 2);

    SweepDesc desc;
    Mesh mesh;
    NurbsSurface outSurface;
    SweepLoftDiagnostic diag = SweepLoftOperation::sweep(profile, zeroPath, desc, outSurface, &mesh);

    EXPECT_EQ(diag, SweepLoftDiagnostic::PathTooShort);
}

TEST(SweepLoft, DeterministicOutput) {
    NurbsSurface profile = makeCurveLine({-1, 0, 0}, {1, 0, 0});
    NurbsSurface path = makeCurveLine({0, 0, 0}, {0, 0, 4});

    SweepDesc desc;
    desc.crossSectionCount = 5;

    Mesh mesh1, mesh2;
    NurbsSurface out1, out2;

    (void)SweepLoftOperation::sweep(profile, path, desc, out1, &mesh1);
    (void)SweepLoftOperation::sweep(profile, path, desc, out2, &mesh2);

    EXPECT_EQ(mesh1.attributes().vertexCount(), mesh2.attributes().vertexCount());
    EXPECT_EQ(mesh1.topology().faceCount(), mesh2.topology().faceCount());

    const auto& p1 = mesh1.attributes().positions();
    const auto& p2 = mesh2.attributes().positions();
    ASSERT_EQ(p1.size(), p2.size());

    for (size_t i = 0; i < p1.size(); ++i) {
        EXPECT_FLOAT_EQ(p1[i].x, p2[i].x);
        EXPECT_FLOAT_EQ(p1[i].y, p2[i].y);
        EXPECT_FLOAT_EQ(p1[i].z, p2[i].z);
    }

    const auto& t1 = mesh1.topology();
    const auto& t2 = mesh2.topology();
    ASSERT_EQ(t1.faceCount(), t2.faceCount());
    for (size_t i = 0; i < t1.faceCount(); ++i) {
        const auto& f1 = t1.face(i);
        const auto& f2 = t2.face(i);
        ASSERT_EQ(f1.indices.size(), f2.indices.size());
        for (size_t j = 0; j < f1.indices.size(); ++j) {
            EXPECT_EQ(f1.indices[j], f2.indices[j]);
        }
    }
}

TEST(SweepLoft, BothInputsInvalidReturnsProfileErrorFirst) {
    NurbsSurface invalidProfile;
    NurbsSurface invalidPath;

    SweepDesc desc;
    Mesh mesh;
    NurbsSurface outSurface;
    SweepLoftDiagnostic diag = SweepLoftOperation::sweep(invalidProfile, invalidPath, desc, outSurface, &mesh);

    EXPECT_EQ(diag, SweepLoftDiagnostic::InvalidProfileCurve);
}

TEST(SweepLoft, MeshOutputIsValid) {
    NurbsSurface profile = makeCurveLine({-1, 0, 0}, {1, 0, 0});
    NurbsSurface path = makeCurveLine({0, 0, 0}, {0, 0, 4});

    SweepDesc desc;
    desc.crossSectionCount = 4;

    Mesh mesh;
    NurbsSurface outSurface;
    SweepLoftDiagnostic diag = SweepLoftOperation::sweep(profile, path, desc, outSurface, &mesh);

    EXPECT_EQ(diag, SweepLoftDiagnostic::Success);
    EXPECT_TRUE(mesh.isValid());
}

TEST(SweepLoft, NurbsSurfaceOutputWhenRequested) {
    NurbsSurface profile = makeCurveLine({-1, 0, 0}, {1, 0, 0});
    NurbsSurface path = makeCurveLine({0, 0, 0}, {0, 0, 4});

    SweepDesc desc;
    desc.crossSectionCount = 4;
    desc.outputAsNurbsSurface = true;

    Mesh mesh;
    NurbsSurface outSurface;
    SweepLoftDiagnostic diag = SweepLoftOperation::sweep(profile, path, desc, outSurface, &mesh);

    EXPECT_EQ(diag, SweepLoftDiagnostic::Success);
    EXPECT_TRUE(outSurface.isValid());
}

TEST(SweepLoft, SweepWithScaling) {
    NurbsSurface profile = makeCurveLine({-1, 0, 0}, {1, 0, 0});
    NurbsSurface path = makeCurveLine({0, 0, 0}, {0, 0, 4});

    SweepDesc desc;
    desc.crossSectionCount = 2;
    desc.startScale = 1.f;
    desc.endScale = 2.f;

    Mesh mesh;
    NurbsSurface outSurface;
    SweepLoftDiagnostic diag = SweepLoftOperation::sweep(profile, path, desc, outSurface, &mesh);

    EXPECT_EQ(diag, SweepLoftDiagnostic::Success);

    EXPECT_EQ(mesh.attributes().vertexCount(), 4u);
    EXPECT_EQ(mesh.topology().faceCount(), 2u);

    const auto& pos = mesh.attributes().positions();
    ASSERT_EQ(pos.size(), 4u);

    // Second cross-section should be scaled by 2x compared to first
    // First cross-section: profile at (-1,0,0) and (1,0,0) at z=0
    // Second cross-section: profile at (-2,0,4) and (2,0,4) scaled 2x at z=4
    const auto& v0 = pos[0];
    const auto& v1 = pos[1];
    const auto& v2 = pos[2];
    const auto& v3 = pos[3];

    float maxAbsX = 0.f;
    for (const auto& v : pos) {
        if (std::abs(v.x) > maxAbsX) maxAbsX = std::abs(v.x);
    }
    EXPECT_GT(maxAbsX, 1.5f);
}

} // namespace
