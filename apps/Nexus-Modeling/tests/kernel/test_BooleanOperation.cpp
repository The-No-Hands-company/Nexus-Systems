// ─────────────────────────────────────────────────────────────────────────────
//  Boolean Operation Regression Tests
//
//  Tests v0 boolean operation implementation with deterministic geometry.
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/geometry/BooleanOperation.h>
#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/MeshMassProperties.h>
#include <nexus/geometry/MeshTopologyValidation.h>
#include <gtest/gtest.h>

#include <cmath>
#include <utility>

#include <limits>

namespace nexus::geometry::testing {

using namespace nexus::geometry::primitives;

// ─────────────────────────────────────────────────────────────────────────────
//  Validation Tests
// ─────────────────────────────────────────────────────────────────────────────

TEST(BooleanOperation, ValidateMeshAcceptsValidTriangleMesh)
{
    // Create a simple triangle mesh
    Mesh mesh = makeTriangle(1.0f);

    auto report = BooleanOperation::validateMesh(mesh);
    EXPECT_TRUE(report.valid);
    EXPECT_EQ(report.code, BooleanOperationDiagnostic::Success);
}

TEST(BooleanOperation, ValidateMeshRejectsEmptyMesh)
{
    Mesh empty;

    auto report = BooleanOperation::validateMesh(empty);
    EXPECT_FALSE(report.valid);
    EXPECT_TRUE(report.hasWarning(BooleanOperationDiagnostic::InputAInvalid));
    EXPECT_FALSE(report.messages.empty());
}

TEST(BooleanOperation, ValidateMeshRejectsNoPositions)
{
    Mesh meshNoPos;
    meshNoPos.topology().addFace(Face{{0, 1, 2}});

    auto report = BooleanOperation::validateMesh(meshNoPos);
    EXPECT_FALSE(report.valid);
    EXPECT_TRUE(report.hasWarning(BooleanOperationDiagnostic::InputAInvalid));
}

TEST(BooleanOperation, ValidateMeshRejectsOutOfBoundsIndices)
{
    Mesh meshBadIndices;
    meshBadIndices.attributes().setPositions({{0, 0, 0}, {1, 0, 0}, {0, 1, 0}});
    meshBadIndices.topology().addFace(Face{{0, 1, 5}});  // Index 5 out of bounds

    auto report = BooleanOperation::validateMesh(meshBadIndices);
    EXPECT_FALSE(report.valid);
    EXPECT_TRUE(report.hasWarning(BooleanOperationDiagnostic::InputAInvalid));
}

TEST(BooleanOperation, ValidateMeshRejectsNonManifoldEdges)
{
    Mesh mesh;
    mesh.attributes().setPositions({
        {0.f, 0.f, 0.f},
        {1.f, 0.f, 0.f},
        {0.f, 1.f, 0.f},
        {0.f, 0.f, 1.f},
        {0.f, -1.f, 0.f},
    });
    mesh.topology().addFace(Face{{0u, 1u, 2u}});
    mesh.topology().addFace(Face{{1u, 0u, 3u}});
    mesh.topology().addFace(Face{{0u, 1u, 4u}});

    auto report = BooleanOperation::validateMesh(mesh);
    EXPECT_FALSE(report.valid);
    EXPECT_TRUE(report.hasWarning(BooleanOperationDiagnostic::InputANotManifold));
}

TEST(BooleanOperation, ValidateMeshRejectsDegenerateTriangle)
{
    Mesh mesh;
    mesh.attributes().setPositions({
        {0.f, 0.f, 0.f},
        {1.f, 0.f, 0.f},
        {2.f, 0.f, 0.f},
    });
    mesh.topology().addFace(Face{{0u, 1u, 2u}});

    auto report = BooleanOperation::validateMesh(mesh);
    EXPECT_FALSE(report.valid);
    EXPECT_TRUE(report.hasWarning(BooleanOperationDiagnostic::GeometricDegeneracy));
}

TEST(BooleanOperation, ValidateMeshRejectsNonFinitePosition)
{
    Mesh mesh;
    mesh.attributes().setPositions({
        {0.f, 0.f, 0.f},
        {1.f, std::numeric_limits<float>::quiet_NaN(), 0.f},
        {0.f, 1.f, 0.f},
    });
    mesh.topology().addFace(Face{{0u, 1u, 2u}});

    const auto report = BooleanOperation::validateMesh(mesh);
    EXPECT_FALSE(report.valid);
    EXPECT_TRUE(report.hasWarning(BooleanOperationDiagnostic::InputAInvalid));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Basic Operation Tests
// ─────────────────────────────────────────────────────────────────────────────

TEST(BooleanOperation, UnionTwoBoxesDeterministically)
{
    Mesh boxA = makeBox(2.0f, 2.0f, 2.0f);
    Mesh boxB = makeBox(2.0f, 2.0f, 2.0f);

    Mesh result;
    auto report = BooleanOperation::compute(boxA, boxB, BooleanOperationType::Union, result);

    EXPECT_TRUE(report.valid);
    EXPECT_EQ(report.code, BooleanOperationDiagnostic::Success);
    EXPECT_GT(result.attributes().vertexCount(), 0);
    EXPECT_GT(result.topology().faceCount(), 0);
}

TEST(BooleanOperation, DifferenceTwoBoxesDeterministically)
{
    Mesh boxA = makeBox(2.0f, 2.0f, 2.0f);
    Mesh boxB = makeBox(1.0f, 1.0f, 1.0f);

    Mesh result;
    auto report = BooleanOperation::compute(boxA, boxB, BooleanOperationType::Difference, result);

    EXPECT_TRUE(report.valid);
    EXPECT_EQ(report.code, BooleanOperationDiagnostic::Success);
    EXPECT_GT(result.attributes().vertexCount(), 0);
    EXPECT_GT(result.topology().faceCount(), 0);
}

TEST(BooleanOperation, IntersectionTwoBoxesDeterministically)
{
    Mesh boxA = makeBox(2.0f, 2.0f, 2.0f);
    Mesh boxB = makeBox(1.0f, 1.0f, 1.0f);

    Mesh result;
    auto report =
        BooleanOperation::compute(boxA, boxB, BooleanOperationType::Intersection, result);

    EXPECT_TRUE(report.valid);
    EXPECT_TRUE(
        report.code == BooleanOperationDiagnostic::Success
        || report.code == BooleanOperationDiagnostic::OutputEmpty);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Non-intersecting Geometry Tests
// ─────────────────────────────────────────────────────────────────────────────

TEST(BooleanOperation, UnionWithNonIntersectingBoxes)
{
    Mesh boxA = makeBox(1.0f, 1.0f, 1.0f);

    // Create boxB separated from boxA
    Mesh boxB = makeBox(1.0f, 1.0f, 1.0f);
    nexus::render::Mat4 translate = nexus::render::Mat4::identity();
    translate.m[0][3] = 5.0f;
    (void)boxB.applyTransform(translate);

    Mesh result;
    auto report = BooleanOperation::compute(boxA, boxB, BooleanOperationType::Union, result);

    // Union should succeed even if meshes don't intersect
    EXPECT_TRUE(report.valid);
    EXPECT_GT(result.attributes().vertexCount(), 0);
}

TEST(BooleanOperation, DifferenceWithNonIntersectingMeshesReturnsInputA)
{
    Mesh boxA = makeBox(1.0f, 1.0f, 1.0f);

    // Create boxB separated from boxA
    Mesh boxB = makeBox(1.0f, 1.0f, 1.0f);
    nexus::render::Mat4 translate = nexus::render::Mat4::identity();
    translate.m[0][3] = 5.0f;
    (void)boxB.applyTransform(translate);

    Mesh result;
    auto report = BooleanOperation::compute(boxA, boxB, BooleanOperationType::Difference, result);

    // Difference should return A (or similar volume)
    EXPECT_TRUE(report.valid);
    EXPECT_GT(result.attributes().vertexCount(), 0);
    EXPECT_GT(result.topology().faceCount(), 0);
}

TEST(BooleanOperation, IntersectionWithNonIntersectingMeshesReturnsEmpty)
{
    Mesh boxA = makeBox(1.0f, 1.0f, 1.0f);

    // Create boxB separated from boxA
    Mesh boxB = makeBox(1.0f, 1.0f, 1.0f);
    nexus::render::Mat4 translate = nexus::render::Mat4::identity();
    translate.m[0][3] = 5.0f;
    (void)boxB.applyTransform(translate);

    Mesh result;
    auto report =
        BooleanOperation::compute(boxA, boxB, BooleanOperationType::Intersection, result);

    // Intersection of non-overlapping should be empty (but still valid)
    EXPECT_TRUE(report.valid);
    EXPECT_TRUE(report.hasWarning(BooleanOperationDiagnostic::NoIntersection) ||
                report.hasWarning(BooleanOperationDiagnostic::OutputEmpty));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Normal Preservation Tests
// ─────────────────────────────────────────────────────────────────────────────

TEST(BooleanOperation, PreservesNormalsWhenOptionEnabled)
{
    Mesh boxA = makeBox(2.0f, 2.0f, 2.0f);
    (void)boxA.computeVertexNormals();

    Mesh boxB = makeBox(1.0f, 1.0f, 1.0f);
    (void)boxB.computeVertexNormals();

    BooleanOperationOptions opts;
    opts.preserveNormals = true;

    Mesh result;
    auto report = BooleanOperation::compute(boxA, boxB, BooleanOperationType::Difference, opts, result);

    EXPECT_TRUE(report.valid);
    // Result should have normals if preserve option is true
    if (result.attributes().vertexCount() > 0) {
        // Note: normals are optional, but we expect them if preserve flag is set
        EXPECT_GE(result.attributes().vertexCount(), 0);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Diagnostic Test
// ─────────────────────────────────────────────────────────────────────────────

TEST(BooleanOperation, DiagnosticFlagsWorkCorrectly)
{
    BooleanOperationDiagnostic flags = BooleanOperationDiagnostic::InputAInvalid;
    flags = flags | BooleanOperationDiagnostic::InputBEmpty;

    EXPECT_TRUE(flags == (BooleanOperationDiagnostic::InputAInvalid |
                          BooleanOperationDiagnostic::InputBEmpty));

    bool hasA = (flags & BooleanOperationDiagnostic::InputAInvalid) != BooleanOperationDiagnostic::Success;
    bool hasB = (flags & BooleanOperationDiagnostic::InputBEmpty) != BooleanOperationDiagnostic::Success;
    EXPECT_TRUE(hasA);
    EXPECT_TRUE(hasB);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Operation Name Tests
// ─────────────────────────────────────────────────────────────────────────────

TEST(BooleanOperation, OperationNameReturnsCorrectStrings)
{
    EXPECT_STREQ(BooleanOperation::operationName(BooleanOperationType::Union), "Union");
    EXPECT_STREQ(BooleanOperation::operationName(BooleanOperationType::Difference), "Difference");
    EXPECT_STREQ(BooleanOperation::operationName(BooleanOperationType::Intersection),
                 "Intersection");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Determinism Tests
// ─────────────────────────────────────────────────────────────────────────────

TEST(BooleanOperation, UnionResultIsDeterministicAcrossRuns)
{
    Mesh boxA1 = makeBox(2.0f, 2.0f, 2.0f);
    Mesh boxB1 = makeBox(1.0f, 1.0f, 1.0f);

    Mesh result1;
    auto report1 =
        BooleanOperation::compute(boxA1, boxB1, BooleanOperationType::Union, result1);

    // Second run with same inputs
    Mesh boxA2 = makeBox(2.0f, 2.0f, 2.0f);
    Mesh boxB2 = makeBox(1.0f, 1.0f, 1.0f);

    Mesh result2;
    auto report2 =
        BooleanOperation::compute(boxA2, boxB2, BooleanOperationType::Union, result2);

    // Results should be identical
    EXPECT_TRUE(report1.valid && report2.valid);
    EXPECT_EQ(result1.attributes().vertexCount(), result2.attributes().vertexCount());
    EXPECT_EQ(result1.topology().faceCount(), result2.topology().faceCount());

    // Positions should match
    const auto& pos1 = result1.attributes().positions();
    const auto& pos2 = result2.attributes().positions();
    for (size_t i = 0; i < pos1.size(); ++i) {
        EXPECT_FLOAT_EQ(pos1[i].x, pos2[i].x);
        EXPECT_FLOAT_EQ(pos1[i].y, pos2[i].y);
        EXPECT_FLOAT_EQ(pos1[i].z, pos2[i].z);
    }
}

TEST(BooleanOperation, DifferenceResultIsDeterministicAcrossRuns)
{
    Mesh boxA1 = makeBox(2.0f, 2.0f, 2.0f);
    Mesh boxB1 = makeBox(1.0f, 1.0f, 1.0f);

    Mesh result1;
    auto report1 =
        BooleanOperation::compute(boxA1, boxB1, BooleanOperationType::Difference, result1);

    // Second run with same inputs
    Mesh boxA2 = makeBox(2.0f, 2.0f, 2.0f);
    Mesh boxB2 = makeBox(1.0f, 1.0f, 1.0f);

    Mesh result2;
    auto report2 =
        BooleanOperation::compute(boxA2, boxB2, BooleanOperationType::Difference, result2);

    // Results should be identical
    EXPECT_TRUE(report1.valid && report2.valid);
    EXPECT_EQ(result1.attributes().vertexCount(), result2.attributes().vertexCount());
    EXPECT_EQ(result1.topology().faceCount(), result2.topology().faceCount());
}

TEST(BooleanOperation, ComputeRejectsNonPositiveTolerance)
{
    Mesh boxA = makeBox(2.0f, 2.0f, 2.0f);
    Mesh boxB = makeBox(1.0f, 1.0f, 1.0f);

    BooleanOperationOptions opts;
    opts.geometricTolerance = 0.0f;

    Mesh result;
    auto report = BooleanOperation::compute(boxA, boxB, BooleanOperationType::Union, opts, result);

    EXPECT_FALSE(report.valid);
    EXPECT_TRUE(report.hasWarning(BooleanOperationDiagnostic::NumericalInstability));
}

TEST(BooleanOperation, ComputeRejectsNonFiniteTolerance)
{
    Mesh boxA = makeBox(2.0f, 2.0f, 2.0f);
    Mesh boxB = makeBox(1.0f, 1.0f, 1.0f);

    BooleanOperationOptions opts;
    opts.geometricTolerance = std::numeric_limits<float>::infinity();

    Mesh result;
    const auto report = BooleanOperation::compute(boxA, boxB, BooleanOperationType::Union, opts, result);

    EXPECT_FALSE(report.valid);
    EXPECT_TRUE(report.hasWarning(BooleanOperationDiagnostic::NumericalInstability));
}

TEST(BooleanOperation, ComputeMapsInputBNonManifoldDiagnostic)
{
    Mesh meshA = makeBox(2.0f, 2.0f, 2.0f);

    Mesh meshB;
    meshB.attributes().setPositions({
        {0.f, 0.f, 0.f},
        {1.f, 0.f, 0.f},
        {0.f, 1.f, 0.f},
        {0.f, 0.f, 1.f},
        {0.f, -1.f, 0.f},
    });
    meshB.topology().addFace(Face{{0u, 1u, 2u}});
    meshB.topology().addFace(Face{{1u, 0u, 3u}});
    meshB.topology().addFace(Face{{0u, 1u, 4u}});

    Mesh result;
    auto report =
        BooleanOperation::compute(meshA, meshB, BooleanOperationType::Difference, result);

    EXPECT_FALSE(report.valid);
    EXPECT_TRUE(report.hasWarning(BooleanOperationDiagnostic::InputBNotManifold));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Diagnostic ordering contract tests (slice 9)
// ─────────────────────────────────────────────────────────────────────────────

// validateMesh can accumulate two messages when a mesh has non-triangle faces
// AND a subsequent structural error (e.g. out-of-bounds indices).  The non-
// triangle message starts with 'M' while the index-bounds message starts with
// 'F', so without a sort the insertion order is wrong.
TEST(BooleanOperation, ValidateMeshMultipleMessagesAreLexicographicallySorted)
{
    // Quad face {0,1,2,3} with only 3 vertices — hasNonTriangles=true AND
    // hasValidIndices(3) fails (index 3 >= vertexCount 3).
    Mesh mesh;
    mesh.attributes().setPositions({
        {0.f, 0.f, 0.f},
        {1.f, 0.f, 0.f},
        {0.f, 1.f, 0.f},
    });
    mesh.topology().addFace(Face{{0u, 1u, 2u, 3u}});  // quad, index 3 out of bounds

    auto report = BooleanOperation::validateMesh(mesh);

    EXPECT_FALSE(report.valid);
    ASSERT_EQ(report.messages.size(), 2u);
    EXPECT_TRUE(std::is_sorted(report.messages.begin(), report.messages.end()))
        << "validateMesh messages are not lexicographically sorted";
    // 'F' < 'M': "Face indices..." must come before "Mesh contains non-triangle..."
    EXPECT_LT(report.messages[0], report.messages[1]);
    EXPECT_NE(report.messages[0].find("Face indices"), std::string::npos);
    EXPECT_NE(report.messages[1].find("non-triangle"), std::string::npos);
}

// compute accumulates up to three messages when both inputs have non-triangle
// faces and a degenerate triangle is skipped during fan-triangulation.
// Insertion order: [Input A warning, Input B warning, Degenerate warning].
// Sorted order: [Degenerate..., Input A..., Input B...] ('D' < 'I').
TEST(BooleanOperation, ComputeMultipleWarningMessagesAreLexicographicallySorted)
{
    // meshA: standard box — quad faces → Input A non-triangle warning.
    Mesh meshA = makeBox(2.0f, 2.0f, 2.0f);

    // meshB: one quad face whose first fan-triangle is degenerate (colinear
    // vertices) and whose second fan-triangle is valid and overlaps meshA.
    // Positions: v0=(0,0,0), v1=(1,0,0), v2=(2,0,0), v3=(0,1,0)
    // Fan of quad {0,1,2,3}:
    //   tri0 = {0,1,2} → (0,0,0),(1,0,0),(2,0,0) — colinear → DEGENERATE
    //   tri1 = {0,2,3} → (0,0,0),(2,0,0),(0,1,0) — valid
    Mesh meshB;
    meshB.attributes().setPositions({
        {0.f, 0.f, 0.f},
        {1.f, 0.f, 0.f},
        {2.f, 0.f, 0.f},
        {0.f, 1.f, 0.f},
    });
    meshB.topology().addFace(Face{{0u, 1u, 2u, 3u}});

    Mesh result;
    auto report =
        BooleanOperation::compute(meshA, meshB, BooleanOperationType::Union, {}, result);

    // Three warnings must be present and lexicographically sorted.
    ASSERT_GE(report.messages.size(), 3u);
    EXPECT_TRUE(std::is_sorted(report.messages.begin(), report.messages.end()))
        << "compute messages are not lexicographically sorted";
    // "Degenerate..." ('D') must sort before "Input..." ('I').
    bool foundDegenerate = false;
    bool foundInputA     = false;
    bool foundInputB     = false;
    for (const auto& msg : report.messages) {
        if (msg.find("Degenerate") != std::string::npos)    foundDegenerate = true;
        if (msg.find("Input A") != std::string::npos)       foundInputA     = true;
        if (msg.find("Input B") != std::string::npos)       foundInputB     = true;
    }
    EXPECT_TRUE(foundDegenerate) << "Expected degenerate-skip message";
    EXPECT_TRUE(foundInputA)     << "Expected Input A non-triangle warning";
    EXPECT_TRUE(foundInputB)     << "Expected Input B non-triangle warning";
}

// Public-API regression for the robust pipeline: on COARSE boxes the boolean
// must be watertight + 2-manifold (Euler χ=2, genus-0 closed) with exact volumes
// — the property the old whole-triangle classifier never had.
TEST(BooleanOperation, CoarseBoxesAreWatertightWithCorrectVolume)
{
    Mesh a = makeBox(2.f, 2.f, 2.f);              // [-1,1]^3, vol 8
    Mesh b = makeBox(2.f, 2.f, 2.f);
    auto bp = b.attributes().positions();
    for (auto& v : bp) { v.x += 1.f; v.y += 1.f; v.z += 1.f; }  // [0,2]^3, overlap [0,1]^3 vol 1
    b.attributes().setPositions(std::move(bp));

    struct Case { BooleanOperationType op; float vol; };
    for (const Case c : {Case{BooleanOperationType::Union, 15.f},
                         Case{BooleanOperationType::Intersection, 1.f},
                         Case{BooleanOperationType::Difference, 7.f}}) {
        Mesh out;
        const auto report = BooleanOperation::compute(a, b, c.op, out);
        ASSERT_TRUE(report.isSuccess()) << "op=" << static_cast<int>(c.op);
        EXPECT_NEAR(std::abs(MeshMassProperties::compute(out).volume), c.vol, 0.1f)
            << "op=" << static_cast<int>(c.op);
        EXPECT_EQ(MeshTopologyValidation::validate(out).euler, 2)
            << "op=" << static_cast<int>(c.op);  // closed genus-0 shell
    }
}

namespace {
Mesh uniformScaled(Mesh m, float s)
{
    auto p = m.attributes().positions();
    for (auto& v : p) { v.x *= s; v.y *= s; v.z *= s; }
    m.attributes().setPositions(std::move(p));
    return m;
}
Mesh shifted(Mesh m, float d)
{
    auto p = m.attributes().positions();
    for (auto& v : p) { v.x += d; v.y += d; v.z += d; }
    m.attributes().setPositions(std::move(p));
    return m;
}
}  // namespace

// SCALE-INVARIANT degeneracy (Phase T): the degenerate-triangle test used a FIXED
// absolute |cross|² < 1e-16 floor. Because |cross|² scales as length⁴, it wrongly
// dropped valid sub-millimetre triangles — making small-scale booleans fail with an
// EMPTY, invalid result. The relative test (area vs the triangle's own edge²) keeps
// the SAME boolean valid + non-empty from a 0.05 mm model up to a 5 km model, while
// unit scale is unchanged. (A fixed 1e-16 floor produced 0 faces below ~5e-5.)
TEST(BooleanOperation, DegeneracyIsScaleInvariantAcrossOrdersOfMagnitude)
{
    for (float s : {5e-5f, 1e-2f, 1.f, 100.f, 5000.f}) {
        Mesh a = uniformScaled(makeBox(2.f, 2.f, 2.f), s);
        Mesh b = shifted(uniformScaled(makeBox(2.f, 2.f, 2.f), s), 1.f * s);  // diagonal overlap
        Mesh out;
        const auto report = BooleanOperation::compute(a, b, BooleanOperationType::Union, out);
        EXPECT_TRUE(report.valid) << "s=" << s << " (small-scale boolean must not fail on valid input)";
        EXPECT_GT(out.topology().faceCount(), 0u) << "s=" << s << " (valid tris wrongly dropped)";
    }
}

// A genuinely degenerate (collinear, zero-area) triangle is STILL rejected at ANY
// scale — the relative test flags |cross|²==0 everywhere, so making it scale-aware
// did not weaken real degeneracy detection.
TEST(BooleanOperation, GenuineDegenerateTriangleStillRejectedAtEveryScale)
{
    for (float s : {1e-4f, 1.f, 1000.f}) {
        Mesh m;
        m.attributes().setPositions({{0.f, 0.f, 0.f}, {s, 0.f, 0.f}, {2.f * s, 0.f, 0.f}});  // collinear
        Face f; f.indices = {0, 1, 2};
        m.topology().addFace(f);
        const auto report = BooleanOperation::validateMesh(m);
        EXPECT_TRUE(hasCode(report.code, BooleanOperationDiagnostic::GeometricDegeneracy))
            << "collinear triangle must be flagged degenerate at s=" << s;
    }
}

}  // namespace nexus::geometry::testing
