#include <nexus/parametric/ConstraintGraph.h>
#include <nexus/parametric/ParametricSerialization.h>
#include <nexus/parametric/ParametricSolver.h>

#include <gtest/gtest.h>

#include <cmath>
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
