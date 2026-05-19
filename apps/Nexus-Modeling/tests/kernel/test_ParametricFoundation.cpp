#include <nexus/parametric/ConstraintGraph.h>
#include <nexus/parametric/ParametricSamples.h>
#include <nexus/parametric/ParametricSerialization.h>
#include <nexus/parametric/ParametricSolver.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>

using namespace nexus::parametric;

namespace {

double distance3(const ParametricPoint3& a, const ParametricPoint3& b)
{
    const double dx = b.x - a.x;
    const double dy = b.y - a.y;
    const double dz = b.z - a.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

} // namespace

TEST(ParametricFoundation, ConstraintGraphAddRemoveEntitiesAndConstraints)
{
    ConstraintGraph graph;

    const ParametricEntityId e0 = graph.addPoint({0.0, 0.0, 0.0});
    const ParametricEntityId e1 = graph.addPoint({2.0, 0.0, 0.0});

    ASSERT_NE(e0, kInvalidEntityId);
    ASSERT_NE(e1, kInvalidEntityId);
    EXPECT_EQ(e1, e0 + 1u);
    EXPECT_TRUE(graph.hasEntity(e0));
    EXPECT_TRUE(graph.hasEntity(e1));

    const ParametricConstraintId c0 = graph.addDistanceConstraint(e0, e1, 3.0);
    ASSERT_NE(c0, kInvalidConstraintId);
    EXPECT_EQ(graph.distanceConstraintCount(), 1u);

    EXPECT_TRUE(graph.removeEntity(e0));
    EXPECT_FALSE(graph.hasEntity(e0));
    EXPECT_EQ(graph.distanceConstraintCount(), 0u);
}

TEST(ParametricFoundation, DistanceSolverConvergesForSingleConstraint)
{
    ConstraintGraph graph;

    const ParametricEntityId a = graph.addPoint({0.0, 0.0, 0.0});
    const ParametricEntityId b = graph.addPoint({1.0, 0.0, 0.0});
    ASSERT_NE(graph.addDistanceConstraint(a, b, 5.0), kInvalidConstraintId);

    const ParametricSolverReport report = ParametricSolver::solve(graph);

    EXPECT_TRUE(report.converged);
    ASSERT_TRUE(report.errors.empty());

    const ParametricPoint3* pa = graph.point(a);
    const ParametricPoint3* pb = graph.point(b);
    ASSERT_NE(pa, nullptr);
    ASSERT_NE(pb, nullptr);

    EXPECT_NEAR(distance3(*pa, *pb), 5.0, 1e-9);
}

TEST(ParametricFoundation, SerializationRoundTripPreservesGraphState)
{
    ConstraintGraph graph;

    const ParametricEntityId a = graph.addPoint({1.5, 2.0, -0.5});
    const ParametricEntityId b = graph.addPoint({4.5, 2.0, -0.5});
    ASSERT_NE(graph.addDistanceConstraint(a, b, 3.0), kInvalidConstraintId);

    const std::string serialized = ParametricGraphSerializer::serialize(graph);

    ConstraintGraph restored;
    const ParametricSerializationReport report =
        ParametricGraphSerializer::deserialize(serialized, restored);

    EXPECT_TRUE(report.valid);
    ASSERT_TRUE(report.errors.empty());

    EXPECT_EQ(restored.entityCount(), graph.entityCount());
    EXPECT_EQ(restored.distanceConstraintCount(), graph.distanceConstraintCount());

    const std::string reSerialized = ParametricGraphSerializer::serialize(restored);
    EXPECT_EQ(serialized, reSerialized);
}

TEST(ParametricFoundation, ReplaySolveIsDeterministicAcrossLoads)
{
    ConstraintGraph baseline;

    const ParametricEntityId a = baseline.addPoint({0.0, 0.0, 0.0});
    const ParametricEntityId b = baseline.addPoint({2.0, 0.0, 0.0});
    const ParametricEntityId c = baseline.addPoint({5.0, 0.0, 0.0});

    ASSERT_NE(baseline.addDistanceConstraint(a, b, 3.0), kInvalidConstraintId);
    ASSERT_NE(baseline.addDistanceConstraint(b, c, 4.0), kInvalidConstraintId);

    const std::string initial = ParametricGraphSerializer::serialize(baseline);

    ConstraintGraph runA;
    ConstraintGraph runB;
    ASSERT_TRUE(ParametricGraphSerializer::deserialize(initial, runA).valid);
    ASSERT_TRUE(ParametricGraphSerializer::deserialize(initial, runB).valid);

    const ParametricSolverReport reportA = ParametricSolver::solve(runA);
    const ParametricSolverReport reportB = ParametricSolver::solve(runB);
    EXPECT_TRUE(reportA.errors.empty());
    EXPECT_TRUE(reportB.errors.empty());

    const std::string solvedA = ParametricGraphSerializer::serialize(runA);
    const std::string solvedB = ParametricGraphSerializer::serialize(runB);

    EXPECT_EQ(solvedA, solvedB);
}

TEST(ParametricFoundation, DeserializeRejectsInvalidHeader)
{
    ConstraintGraph graph;
    const ParametricSerializationReport report =
        ParametricGraphSerializer::deserialize("BAD_HEADER\nE 1 0 0 0\n", graph);

    EXPECT_FALSE(report.valid);
    ASSERT_FALSE(report.errors.empty());
}

TEST(ParametricFoundation, SolverErrorsAreDeterministicAndSorted)
{
    ConstraintGraph graph;
    ParametricSolverConfig config{};
    config.maxIterations = 0;

    const ParametricSolverReport reportA = ParametricSolver::solve(graph, config);
    const ParametricSolverReport reportB = ParametricSolver::solve(graph, config);

    EXPECT_FALSE(reportA.converged);
    EXPECT_FALSE(reportB.converged);
    EXPECT_EQ(reportA.errors, reportB.errors);
    EXPECT_TRUE(std::is_sorted(reportA.errors.begin(), reportA.errors.end()));
    EXPECT_TRUE(std::is_sorted(reportB.errors.begin(), reportB.errors.end()));
}

TEST(ParametricFoundation, SerializationErrorsAreDeterministicAndSorted)
{
    ConstraintGraph graphA;
    ConstraintGraph graphB;

    const std::string badData =
        "NEXUS_PARAM_GRAPH_V1\n"
        "C BADKIND 0 0 1 2\n"
        "Q invalid_payload\n"
        "E broken_entity\n";

    const ParametricSerializationReport reportA =
        ParametricGraphSerializer::deserialize(badData, graphA);
    const ParametricSerializationReport reportB =
        ParametricGraphSerializer::deserialize(badData, graphB);

    EXPECT_FALSE(reportA.valid);
    EXPECT_FALSE(reportB.valid);
    EXPECT_EQ(reportA.errors, reportB.errors);
    EXPECT_TRUE(std::is_sorted(reportA.errors.begin(), reportA.errors.end()));
    EXPECT_TRUE(std::is_sorted(reportB.errors.begin(), reportB.errors.end()));
}

TEST(ParametricFoundation, CoincidentConstraintForcesPointMatch)
{
    ConstraintGraph graph;

    const ParametricEntityId a = graph.addPoint({2.0, -1.0, 4.0});
    const ParametricEntityId b = graph.addPoint({10.0, 5.0, -9.0});
    ASSERT_NE(graph.addCoincidentConstraint(a, b), kInvalidConstraintId);

    const ParametricSolverReport report = ParametricSolver::solve(graph);
    EXPECT_TRUE(report.converged);
    ASSERT_TRUE(report.errors.empty());

    const ParametricPoint3* pa = graph.point(a);
    const ParametricPoint3* pb = graph.point(b);
    ASSERT_NE(pa, nullptr);
    ASSERT_NE(pb, nullptr);

    EXPECT_NEAR(pa->x, pb->x, 1e-12);
    EXPECT_NEAR(pa->y, pb->y, 1e-12);
    EXPECT_NEAR(pa->z, pb->z, 1e-12);
}

TEST(ParametricFoundation, AxisAlignedDistanceConstrainsSelectedAxisAndCollapsesOthers)
{
    ConstraintGraph graph;

    const ParametricEntityId a = graph.addPoint({1.0, 2.0, 3.0});
    const ParametricEntityId b = graph.addPoint({4.0, 5.0, 6.0});
    ASSERT_NE(graph.addAxisAlignedDistanceConstraint(a, b, Axis::Y, 7.5), kInvalidConstraintId);

    const ParametricSolverReport report = ParametricSolver::solve(graph);
    EXPECT_TRUE(report.converged);
    ASSERT_TRUE(report.errors.empty());

    const ParametricPoint3* pa = graph.point(a);
    const ParametricPoint3* pb = graph.point(b);
    ASSERT_NE(pa, nullptr);
    ASSERT_NE(pb, nullptr);

    EXPECT_NEAR(pb->x, pa->x, 1e-12);
    EXPECT_NEAR(pb->z, pa->z, 1e-12);
    EXPECT_NEAR(pb->y - pa->y, 7.5, 1e-12);
}

TEST(ParametricFoundation, SerializationRoundTripIncludesNewConstraintTypes)
{
    ConstraintGraph graph;
    const ParametricEntityId a = graph.addPoint({0.0, 0.0, 0.0});
    const ParametricEntityId b = graph.addPoint({1.0, 2.0, 3.0});
    const ParametricEntityId c = graph.addPoint({2.0, 4.0, 6.0});

    ASSERT_NE(graph.addCoincidentConstraint(a, b), kInvalidConstraintId);
    ASSERT_NE(graph.addAxisAlignedDistanceConstraint(b, c, Axis::X, 5.0), kInvalidConstraintId);

    const std::string serialized = ParametricGraphSerializer::serialize(graph);

    ConstraintGraph restored;
    const ParametricSerializationReport report =
        ParametricGraphSerializer::deserialize(serialized, restored);
    EXPECT_TRUE(report.valid);
    ASSERT_TRUE(report.errors.empty());

    EXPECT_EQ(restored.coincidentConstraintCount(), 1u);
    EXPECT_EQ(restored.axisAlignedDistanceConstraintCount(), 1u);
}

TEST(ParametricFoundation, SketchSampleGeneratorBuildsAndSolvesRectangleLikeModel)
{
    SketchSampleModel sample = ParametricSampleGenerator::makeSketchRectangle(8.0, 3.0);

    ASSERT_NE(sample.origin, kInvalidEntityId);
    ASSERT_NE(sample.xHandle, kInvalidEntityId);
    ASSERT_NE(sample.yHandle, kInvalidEntityId);
    ASSERT_NE(sample.corner, kInvalidEntityId);

    const ParametricSolverReport report = ParametricSampleGenerator::solveSketchRectangle(sample);
    EXPECT_TRUE(report.errors.empty());
    EXPECT_TRUE(report.converged);

    const ParametricPoint3* origin = sample.graph.point(sample.origin);
    const ParametricPoint3* xHandle = sample.graph.point(sample.xHandle);
    const ParametricPoint3* yHandle = sample.graph.point(sample.yHandle);
    const ParametricPoint3* corner = sample.graph.point(sample.corner);
    ASSERT_NE(origin, nullptr);
    ASSERT_NE(xHandle, nullptr);
    ASSERT_NE(yHandle, nullptr);
    ASSERT_NE(corner, nullptr);

    EXPECT_NEAR(xHandle->x - origin->x, 8.0, 1e-12);
    EXPECT_NEAR(yHandle->y - origin->y, 3.0, 1e-12);
    EXPECT_NEAR(corner->x, xHandle->x, 1e-12);
    EXPECT_NEAR(corner->y, yHandle->y, 1e-12);
    EXPECT_NEAR(corner->z, origin->z, 1e-12);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Non-finite distance input hardening
// ─────────────────────────────────────────────────────────────────────────────

TEST(ParametricFoundation, AddDistanceConstraintRejectsNaNDistance)
{
    ConstraintGraph graph;
    const ParametricEntityId a = graph.addPoint({0.0, 0.0, 0.0});
    const ParametricEntityId b = graph.addPoint({1.0, 0.0, 0.0});

    const double nan = std::numeric_limits<double>::quiet_NaN();
    EXPECT_EQ(graph.addDistanceConstraint(a, b, nan), kInvalidConstraintId);
    EXPECT_EQ(graph.distanceConstraintCount(), 0u);
}

TEST(ParametricFoundation, AddDistanceConstraintRejectsPositiveInfDistance)
{
    ConstraintGraph graph;
    const ParametricEntityId a = graph.addPoint({0.0, 0.0, 0.0});
    const ParametricEntityId b = graph.addPoint({1.0, 0.0, 0.0});

    const double inf = std::numeric_limits<double>::infinity();
    EXPECT_EQ(graph.addDistanceConstraint(a, b, inf), kInvalidConstraintId);
    EXPECT_EQ(graph.distanceConstraintCount(), 0u);
}

TEST(ParametricFoundation, AddDistanceConstraintRejectsNegativeDistance)
{
    ConstraintGraph graph;
    const ParametricEntityId a = graph.addPoint({0.0, 0.0, 0.0});
    const ParametricEntityId b = graph.addPoint({1.0, 0.0, 0.0});

    EXPECT_EQ(graph.addDistanceConstraint(a, b, -1.0), kInvalidConstraintId);
    EXPECT_EQ(graph.distanceConstraintCount(), 0u);
}

TEST(ParametricFoundation, AddAxisAlignedDistanceConstraintRejectsNaNDistance)
{
    ConstraintGraph graph;
    const ParametricEntityId a = graph.addPoint({0.0, 0.0, 0.0});
    const ParametricEntityId b = graph.addPoint({1.0, 0.0, 0.0});

    const double nan = std::numeric_limits<double>::quiet_NaN();
    EXPECT_EQ(graph.addAxisAlignedDistanceConstraint(a, b, Axis::X, nan), kInvalidConstraintId);
    EXPECT_EQ(graph.axisAlignedDistanceConstraintCount(), 0u);
}

TEST(ParametricFoundation, AddAxisAlignedDistanceConstraintRejectsPositiveInfDistance)
{
    ConstraintGraph graph;
    const ParametricEntityId a = graph.addPoint({0.0, 0.0, 0.0});
    const ParametricEntityId b = graph.addPoint({1.0, 0.0, 0.0});

    const double inf = std::numeric_limits<double>::infinity();
    EXPECT_EQ(graph.addAxisAlignedDistanceConstraint(a, b, Axis::Y, inf), kInvalidConstraintId);
    EXPECT_EQ(graph.axisAlignedDistanceConstraintCount(), 0u);
}

TEST(ParametricFoundation, AddAxisAlignedDistanceConstraintRejectsNegativeDistance)
{
    ConstraintGraph graph;
    const ParametricEntityId a = graph.addPoint({0.0, 0.0, 0.0});
    const ParametricEntityId b = graph.addPoint({1.0, 0.0, 0.0});

    EXPECT_EQ(graph.addAxisAlignedDistanceConstraint(a, b, Axis::Z, -5.0), kInvalidConstraintId);
    EXPECT_EQ(graph.axisAlignedDistanceConstraintCount(), 0u);
}
