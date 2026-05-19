#include <gtest/gtest.h>
#include "nexus/sim/FluidSolver.h"

using namespace nexus;

// ── FluidSolver particle management ──────────────────────────────────────────

TEST(FluidSolver, AddParticleReturnsUniqueIds) {
    FluidSolver solver;
    const auto a = solver.addParticle({1.0f, {0,0,0}, {0,0,0}, 1000.0f});
    const auto b = solver.addParticle({1.0f, {1,0,0}, {0,0,0}, 1000.0f});
    EXPECT_NE(a, kInvalidFluidParticleId);
    EXPECT_NE(b, kInvalidFluidParticleId);
    EXPECT_NE(a, b);
    EXPECT_EQ(solver.particleCount(), 2u);
}

TEST(FluidSolver, RemoveParticleReducesCount) {
    FluidSolver solver;
    const auto id = solver.addParticle({1.0f, {}, {}, 1000.0f});
    ASSERT_TRUE(solver.hasParticle(id));
    EXPECT_TRUE(solver.removeParticle(id));
    EXPECT_FALSE(solver.hasParticle(id));
    EXPECT_EQ(solver.particleCount(), 0u);
}

TEST(FluidSolver, RemoveUnknownParticleReturnsFalse) {
    FluidSolver solver;
    EXPECT_FALSE(solver.removeParticle(99u));
}

TEST(FluidSolver, StaticParticleNotReturnedByGetState) {
    FluidSolver solver;
    const auto id = solver.addParticle({0.0f, {1,2,3}, {0,0,0}, 1000.0f});  // static
    FluidVec3 pos{}, vel{};
    float density = 0.0f;
    EXPECT_FALSE(solver.getParticleState(id, pos, vel, density));
}

// ── FluidSolver parameters ────────────────────────────────────────────────────

TEST(FluidSolver, GravityDefaultAndSetter) {
    FluidSolver solver;
    EXPECT_FLOAT_EQ(solver.gravity().y, -9.81f);
    solver.setGravity({0,0,0});
    EXPECT_FLOAT_EQ(solver.gravity().y, 0.0f);
}

TEST(FluidSolver, SmoothingRadiusDefaultAndSetter) {
    FluidSolver solver;
    EXPECT_FLOAT_EQ(solver.smoothingRadius(), 0.1f);
    solver.setSmoothingRadius(0.5f);
    EXPECT_FLOAT_EQ(solver.smoothingRadius(), 0.5f);
}

TEST(FluidSolver, SmoothingRadiusRejectsNonPositiveValues) {
    FluidSolver solver;
    solver.setSmoothingRadius(0.5f);
    EXPECT_FLOAT_EQ(solver.smoothingRadius(), 0.5f);

    solver.setSmoothingRadius(0.0f);
    EXPECT_FLOAT_EQ(solver.smoothingRadius(), 0.5f);

    solver.setSmoothingRadius(-0.1f);
    EXPECT_FLOAT_EQ(solver.smoothingRadius(), 0.5f);
}

TEST(FluidSolver, PressureStiffnessDefaultAndSetter) {
    FluidSolver solver;
    EXPECT_FLOAT_EQ(solver.pressureStiffness(), 200.0f);
    solver.setPressureStiffness(500.0f);
    EXPECT_FLOAT_EQ(solver.pressureStiffness(), 500.0f);
}

// ── FluidSolver step ──────────────────────────────────────────────────────────

TEST(FluidSolver, StepWithNegativeDtFails) {
    FluidSolver solver;
    const auto id = solver.addParticle({1.0f, {0,0,0}, {0,0,0}, 1000.0f});
    EXPECT_NE(id, kInvalidFluidParticleId);
    const auto r = solver.step(-0.016);
    EXPECT_FALSE(r.ok);
    EXPECT_EQ(r.particlesAdvanced, 0u);
}

TEST(FluidSolver, StepAdvancesDynamicParticles) {
    FluidSolver solver;
    solver.setGravity({0.0f, 0.0f, 0.0f});
    const auto id = solver.addParticle({1.0f, {0,1,0}, {0,0,0}, 1000.0f});
    const auto r  = solver.step(0.016);
    EXPECT_TRUE(r.ok);
    EXPECT_EQ(r.particlesAdvanced, 1u);
    EXPECT_GT(r.simulationTime, 0.0);
    FluidVec3 pos{}, vel{};
    float density = 0.0f;
    EXPECT_TRUE(solver.getParticleState(id, pos, vel, density));
}

TEST(FluidSolver, StaticParticleNotAdvanced) {
    FluidSolver solver;
    solver.setGravity({0.0f, -9.81f, 0.0f});
    const auto staticId = solver.addParticle({0.0f, {0,5,0}, {0,0,0}, 1000.0f}); // static
    const auto dynamic = solver.addParticle({1.0f, {0,5,0}, {0,0,0}, 1000.0f});
    EXPECT_NE(staticId, kInvalidFluidParticleId);
    const auto r = solver.step(0.016);
    EXPECT_EQ(r.particlesAdvanced, 1u);

    FluidVec3 pos{}, vel{};
    float density = 0.0f;
    ASSERT_TRUE(solver.getParticleState(dynamic, pos, vel, density));
    // Dynamic particle fell under gravity.
    EXPECT_LT(pos.y, 5.0f);
}

// ── FluidSolver snapshot/rollback ─────────────────────────────────────────────

TEST(FluidSolver, CaptureAndRestorePreservesState) {
    FluidSolver solver;
    solver.setGravity({0.0f, -9.81f, 0.0f});
    const auto id = solver.addParticle({1.0f, {0,10,0}, {0,0,0}, 1000.0f});
    EXPECT_NE(id, kInvalidFluidParticleId);
    EXPECT_TRUE(solver.step(0.016).ok);

    const FluidState snap = solver.captureState();
    EXPECT_TRUE(solver.step(0.016).ok);
    EXPECT_TRUE(solver.step(0.016).ok);

    ASSERT_TRUE(solver.restoreState(snap));
    const FluidState after = solver.captureState();
    EXPECT_TRUE(snap == after);
}

TEST(FluidSolver, DeterministicReplayProducesSameState) {
    auto runSolver = []() -> FluidState {
        FluidSolver solver;
        solver.setGravity({0.0f, -9.81f, 0.0f});
        const auto a = solver.addParticle({1.0f, {0,5,0}, {0,0,0}, 1000.0f});
        const auto b = solver.addParticle({2.0f, {0.05f,5,0}, {0,0,0}, 1000.0f});
        if (a == kInvalidFluidParticleId || b == kInvalidFluidParticleId) {
            return {};
        }
        for (int i = 0; i < 10; ++i) {
            if (!solver.step(0.016).ok) {
                return {};
            }
        }
        return solver.captureState();
    };
    const FluidState r1 = runSolver();
    const FluidState r2 = runSolver();
    EXPECT_TRUE(r1 == r2);
}

// ── FluidState serialization ──────────────────────────────────────────────────

TEST(FluidSolver, SerializationRoundTripIsDeterministic) {
    FluidSolver solver;
    solver.setGravity({0.0f, -9.81f, 0.0f});
    // Insert in reverse id order to verify sorted serialization.
    const auto b = solver.addParticle({2.0f, {0.1f,5,0}, {0,0,0}, 1000.0f});
    const auto a = solver.addParticle({1.0f, {0,5,0},    {0,0,0}, 1000.0f});
    EXPECT_GT(b, 0u);
    EXPECT_GT(a, 0u);
    EXPECT_TRUE(solver.step(0.032).ok);

    const FluidState snap    = solver.captureState();
    const auto       bytesA  = serializeFluidState(snap);
    const auto       bytesB  = serializeFluidState(snap);
    EXPECT_EQ(bytesA, bytesB);

    FluidState restored;
    ASSERT_TRUE(deserializeFluidState(bytesA, restored));
    EXPECT_TRUE(snap == restored);

    const auto bytesC = serializeFluidState(restored);
    EXPECT_EQ(bytesC, bytesA);
}

TEST(FluidSolver, DeserializeRejectsMalformedBlob) {
    FluidState out;
    EXPECT_FALSE(deserializeFluidState({}, out));
    // Partial header (5 bytes).
    EXPECT_FALSE(deserializeFluidState({0x46, 0x44, 0x4c, 0x31, 0x01}, out));
}
