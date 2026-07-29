#include <gtest/gtest.h>
#include "nexus/sim/FluidSolver.h"

#include <cmath>
#include <limits>

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

TEST(FluidSolver, AddParticleRejectsNegativeMassOrDensity) {
    FluidSolver solver;

    EXPECT_EQ(solver.addParticle({-1.0f, {0,0,0}, {0,0,0}, 1000.0f}), kInvalidFluidParticleId);
    EXPECT_EQ(solver.addParticle({1.0f, {0,0,0}, {0,0,0}, -1000.0f}), kInvalidFluidParticleId);
    EXPECT_EQ(solver.particleCount(), 0u);
}

TEST(FluidSolver, AddParticleRejectsNonFiniteState) {
    FluidSolver solver;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();

    EXPECT_EQ(solver.addParticle({nan, {0,0,0}, {0,0,0}, 1000.0f}), kInvalidFluidParticleId);
    EXPECT_EQ(solver.addParticle({1.0f, {inf,0,0}, {0,0,0}, 1000.0f}), kInvalidFluidParticleId);
    EXPECT_EQ(solver.addParticle({1.0f, {0,0,0}, {0,nan,0}, 1000.0f}), kInvalidFluidParticleId);
    EXPECT_EQ(solver.addParticle({1.0f, {0,0,0}, {0,0,0}, inf}), kInvalidFluidParticleId);
    EXPECT_EQ(solver.particleCount(), 0u);
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

TEST(FluidSolver, PressureStiffnessRejectsNegativeValue) {
    FluidSolver solver;
    solver.setPressureStiffness(250.0f);
    EXPECT_FLOAT_EQ(solver.pressureStiffness(), 250.0f);

    solver.setPressureStiffness(-1.0f);
    EXPECT_FLOAT_EQ(solver.pressureStiffness(), 250.0f);
}

TEST(FluidSolver, PressureStiffnessRejectsNonFiniteValues) {
    FluidSolver solver;
    solver.setPressureStiffness(250.0f);
    EXPECT_FLOAT_EQ(solver.pressureStiffness(), 250.0f);

    solver.setPressureStiffness(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(solver.pressureStiffness(), 250.0f);

    solver.setPressureStiffness(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(solver.pressureStiffness(), 250.0f);
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

TEST(FluidSolver, StepRejectsNonFiniteRuntimeState) {
    FluidSolver solver;
    const auto id = solver.addParticle({1.0f, {0,0,0}, {0,0,0}, 1000.0f});
    ASSERT_NE(id, kInvalidFluidParticleId);

    solver.setGravity({0.0f, std::numeric_limits<float>::infinity(), 0.0f});

    const auto r = solver.step(0.016);
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

TEST(FluidSolver, RestoreStateRejectsNonFiniteSnapshotData) {
    FluidSolver solver;
    const auto id = solver.addParticle({1.0f, {0,1,0}, {0,0,0}, 1000.0f});
    ASSERT_NE(id, kInvalidFluidParticleId);

    const FluidState baseline = solver.captureState();

    FluidState bad = baseline;
    bad.simulationTime = std::numeric_limits<double>::quiet_NaN();
    bad.particles[0].velocity.y = std::numeric_limits<float>::infinity();
    bad.particles[0].density = std::numeric_limits<float>::quiet_NaN();

    EXPECT_FALSE(solver.restoreState(bad));

    const FluidState after = solver.captureState();
    EXPECT_TRUE(after == baseline);
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

// ─────────────────────────────────────────────────────────────────────────────
//  setSmoothingRadius rejects non-finite input.
//
//  Every other setter on this solver guards with the file's bit-inspecting isFiniteFloat;
//  this one was written with std::isfinite, which under the kernel's former -ffast-math
//  build reported NaN and Inf as finite. A NaN smoothing radius is not a contained fault:
//  h is the denominator of every SPH kernel weight, so one accepted NaN turns the whole
//  particle field non-finite on the next step. The flag is gone and the guard now matches
//  its neighbours; this pins it.
// ─────────────────────────────────────────────────────────────────────────────
TEST(FluidSolver, SmoothingRadiusRejectsNonFiniteInput)
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();

    FluidSolver solver;
    solver.setSmoothingRadius(0.5f);
    ASSERT_FLOAT_EQ(solver.smoothingRadius(), 0.5f);

    for (const float bad : {nan, inf, -inf, 0.f, -1.f}) {
        solver.setSmoothingRadius(bad);
        EXPECT_FLOAT_EQ(solver.smoothingRadius(), 0.5f)
            << "a rejected smoothing radius was stored anyway";
    }

    // The guard rejects; it does not disable the setter.
    solver.setSmoothingRadius(0.25f);
    EXPECT_FLOAT_EQ(solver.smoothingRadius(), 0.25f);
}
