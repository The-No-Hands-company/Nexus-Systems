// ─────────────────────────────────────────────────────────────────────────────
//  Test: Assembly Solver — 3D assembly constraint resolution
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/agent/AssemblySolver.h>

#include <gtest/gtest.h>

#include <cmath>

using namespace nexus::agent;

TEST(AssemblySolver, NoConstraintsConvergesImmediately)
{
    AssemblySolver solver;
    solver.addBody(AssemblyBodyState{});
    solver.addBody(AssemblyBodyState{});
    auto result = solver.solve();
    EXPECT_TRUE(result.converged);
    EXPECT_EQ(result.iterations, 0u);
}

TEST(AssemblySolver, MateConstraint)
{
    AssemblySolver solver;
    AssemblyBodyState bodyA;
    bodyA.fixed = true;
    AssemblyBodyState bodyB;
    bodyB.translation = {5, 0, 0};
    solver.addBody(bodyA);
    solver.addBody(bodyB);
    AssemblyConstraint c;
    c.type = AssemblyConstraintType::Mate;
    c.bodyA = 0;
    c.bodyB = 1;
    c.pointA = {0, 0, 0};
    c.pointB = {0, 0, 0};
    solver.addConstraint(c);
    auto result = solver.solve(200, 1e-3f);
    EXPECT_TRUE(result.converged);
    // Body B should have been moved toward body A
    EXPECT_LT(std::abs(result.bodyStates[1].translation.x - 0.0f), 1.0f);
}

TEST(AssemblySolver, DistanceConstraint)
{
    AssemblySolver solver;
    AssemblyBodyState bodyA;
    bodyA.fixed = true;
    AssemblyBodyState bodyB;
    bodyB.translation = {10, 0, 0};
    solver.addBody(bodyA);
    solver.addBody(bodyB);
    AssemblyConstraint c;
    c.type = AssemblyConstraintType::Distance;
    c.bodyA = 0;
    c.bodyB = 1;
    c.pointA = {0, 0, 0};
    c.pointB = {0, 0, 0};
    c.value = 3.0f;
    solver.addConstraint(c);
    auto result = solver.solve(200, 1e-2f);
    EXPECT_TRUE(result.converged);
    float actualDist = result.bodyStates[1].translation.x;
    EXPECT_NEAR(actualDist, 3.0f, 0.1f);
}

TEST(AssemblySolver, FixedBodyDoesNotMove)
{
    AssemblySolver solver;
    AssemblyBodyState bodyA;
    bodyA.fixed = true;
    bodyA.translation = {0, 0, 0};
    AssemblyBodyState bodyB;
    bodyB.translation = {2, 0, 0};
    solver.addBody(bodyA);
    solver.addBody(bodyB);
    solver.setBodyFixed(0, true);
    AssemblyConstraint c;
    c.type = AssemblyConstraintType::Mate;
    c.bodyA = 0;
    c.bodyB = 1;
    c.pointA = {0, 0, 0};
    c.pointB = {0, 0, 0};
    solver.addConstraint(c);
    auto result = solver.solve(100, 1e-3f);
    // Body A should not have moved
    EXPECT_FLOAT_EQ(result.bodyStates[0].translation.x, 0.0f);
    EXPECT_FLOAT_EQ(result.bodyStates[0].translation.y, 0.0f);
    EXPECT_FLOAT_EQ(result.bodyStates[0].translation.z, 0.0f);
}

TEST(AssemblySolver, AlignConstraintAxis)
{
    AssemblySolver solver;
    AssemblyBodyState bodyA;
    bodyA.fixed = true;
    AssemblyBodyState bodyB;
    solver.addBody(bodyA);
    solver.addBody(bodyB);
    AssemblyConstraint c;
    c.type = AssemblyConstraintType::Align;
    c.bodyA = 0;
    c.bodyB = 1;
    c.axisA = {0, 1, 0};
    c.axisB = {0, 1, 0};
    solver.addConstraint(c);
    auto result = solver.solve(50, 1e-3f);
    EXPECT_TRUE(result.converged);
    EXPECT_LT(result.maxError, 0.01f);
}

TEST(AssemblySolver, BodyCount)
{
    AssemblySolver solver;
    EXPECT_EQ(solver.bodyCount(), 0u);
    solver.addBody(AssemblyBodyState{});
    EXPECT_EQ(solver.bodyCount(), 1u);
    solver.addBody(AssemblyBodyState{});
    EXPECT_EQ(solver.bodyCount(), 2u);
}

TEST(AssemblySolver, ConstraintCount)
{
    AssemblySolver solver;
    EXPECT_EQ(solver.constraintCount(), 0u);
    solver.addBody(AssemblyBodyState{});
    solver.addBody(AssemblyBodyState{});
    AssemblyConstraint c;
    c.type = AssemblyConstraintType::Mate;
    c.bodyA = 0;
    c.bodyB = 1;
    solver.addConstraint(c);
    EXPECT_EQ(solver.constraintCount(), 1u);
}

TEST(AssemblySolver, Clear)
{
    AssemblySolver solver;
    solver.addBody(AssemblyBodyState{});
    solver.addConstraint(AssemblyConstraint{});
    solver.clear();
    EXPECT_EQ(solver.bodyCount(), 0u);
    EXPECT_EQ(solver.constraintCount(), 0u);
}

TEST(AssemblySolver, GearConstraintError)
{
    AssemblySolver solver;
    solver.addBody(AssemblyBodyState{});
    solver.addBody(AssemblyBodyState{Vec3{5, 0, 0}});
    AssemblyConstraint c;
    c.type = AssemblyConstraintType::Gear;
    c.bodyA = 0;
    c.bodyB = 1;
    c.pointA = {0, 0, 0};
    c.pointB = {0, 0, 0};
    c.value = 5.0f; // target distance = rA + rB
    solver.addConstraint(c);
    auto result = solver.solve(100, 1e-2f);
    EXPECT_TRUE(result.converged);
}
