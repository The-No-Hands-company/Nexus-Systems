#include <gtest/gtest.h>
#include "nexus/sim/SimulationCore.h"

#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <tuple>
#include <vector>

using namespace nexus;

// ── Helpers ───────────────────────────────────────────────────────────────────

static bool approxEq(float a, float b, float eps = 1e-4f) {
    return std::fabs(a - b) <= eps;
}

static bool vec3Eq(const SimVec3& a, const SimVec3& b, float eps = 1e-4f) {
    return approxEq(a.x, b.x, eps) && approxEq(a.y, b.y, eps) && approxEq(a.z, b.z, eps);
}

// ── Body management ───────────────────────────────────────────────────────────

TEST(SimulationCore, EmptySolverHasZeroBodies) {
    RigidBodySolver s;
    EXPECT_EQ(s.bodyCount(), 0u);
}

TEST(SimulationCore, AddBodyIncreasesCount) {
    RigidBodySolver s;
    (void)s.addBody({1.0f});
    EXPECT_EQ(s.bodyCount(), 1u);
}

TEST(SimulationCore, AddedBodyIsKnown) {
    RigidBodySolver s;
    BodyId id = s.addBody({1.0f});
    EXPECT_TRUE(s.hasBody(id));
    EXPECT_NE(id, kInvalidBodyId);
}

TEST(SimulationCore, UnknownBodyReturnsFalse) {
    RigidBodySolver s;
    EXPECT_FALSE(s.hasBody(99u));
}

TEST(SimulationCore, BodyIdsAreUniqueAndNonZero) {
    RigidBodySolver s;
    BodyId a = s.addBody({1.0f});
    BodyId b = s.addBody({2.0f});
    EXPECT_NE(a, b);
    EXPECT_NE(a, kInvalidBodyId);
    EXPECT_NE(b, kInvalidBodyId);
}

TEST(SimulationCore, RemoveBodyDecreasesCount) {
    RigidBodySolver s;
    BodyId id = s.addBody({1.0f});
    EXPECT_TRUE(s.removeBody(id));
    EXPECT_EQ(s.bodyCount(), 0u);
    EXPECT_FALSE(s.hasBody(id));
}

TEST(SimulationCore, RemoveUnknownBodyReturnsFalse) {
    RigidBodySolver s;
    EXPECT_FALSE(s.removeBody(99u));
}

TEST(SimulationCore, GetBodyStateReturnsInitialValues) {
    RigidBodySolver s;
    SimBodyDesc desc;
    desc.mass     = 2.0f;
    desc.position = {1.0f, 2.0f, 3.0f};
    desc.velocity = {0.1f, 0.2f, 0.3f};
    BodyId id = s.addBody(desc);

    SimVec3 pos, vel;
    EXPECT_TRUE(s.getBodyState(id, pos, vel));
    EXPECT_TRUE(vec3Eq(pos, {1.0f, 2.0f, 3.0f}));
    EXPECT_TRUE(vec3Eq(vel, {0.1f, 0.2f, 0.3f}));
}

TEST(SimulationCore, GetBodyStateReturnsFalseForUnknownId) {
    RigidBodySolver s;
    SimVec3 p, v;
    EXPECT_FALSE(s.getBodyState(99u, p, v));
}

TEST(SimulationCore, AddBodyRejectsNegativeMass) {
    RigidBodySolver s;
    EXPECT_EQ(s.addBody({-1.0f}), kInvalidBodyId);
    EXPECT_EQ(s.bodyCount(), 0u);
}

TEST(SimulationCore, AddBodyRejectsNonFiniteState) {
    RigidBodySolver s;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();

    SimBodyDesc badMass;
    badMass.mass = nan;
    EXPECT_EQ(s.addBody(badMass), kInvalidBodyId);

    SimBodyDesc badPosition;
    badPosition.mass = 1.0f;
    badPosition.position = {inf, 0.0f, 0.0f};
    EXPECT_EQ(s.addBody(badPosition), kInvalidBodyId);

    SimBodyDesc badVelocity;
    badVelocity.mass = 1.0f;
    badVelocity.velocity = {0.0f, nan, 0.0f};
    EXPECT_EQ(s.addBody(badVelocity), kInvalidBodyId);

    EXPECT_EQ(s.bodyCount(), 0u);
}

TEST(SimulationCore, ClearRemovesAllBodies) {
    RigidBodySolver s;
    (void)s.addBody({1.0f});
    (void)s.addBody({2.0f});
    s.clear();
    EXPECT_EQ(s.bodyCount(), 0u);
}

TEST(SimulationCore, ClearResetsSimulationTime) {
    RigidBodySolver s;
    (void)s.addBody({1.0f});
    (void)s.step(0.016);
    s.clear();
    EXPECT_DOUBLE_EQ(s.simulationTime(), 0.0);
}

// ── Static bodies ─────────────────────────────────────────────────────────────

TEST(SimulationCore, StaticBodyIsNotIntegrated) {
    RigidBodySolver s;
    SimBodyDesc desc;
    desc.mass     = 0.0f; // static
    desc.position = {0.0f, 10.0f, 0.0f};
    BodyId id = s.addBody(desc);

    auto r = s.step(1.0);
    EXPECT_EQ(r.bodiesIntegrated, 0u);

    SimVec3 pos, vel;
    ASSERT_TRUE(s.getBodyState(id, pos, vel));
    EXPECT_TRUE(vec3Eq(pos, {0.0f, 10.0f, 0.0f}));
}

TEST(SimulationCore, ApplyForceReturnsFalseForStaticBody) {
    RigidBodySolver s;
    BodyId id = s.addBody({0.0f}); // static
    EXPECT_FALSE(s.applyForce(id, {0.0f, 100.0f, 0.0f}));
}

// ── Gravity ───────────────────────────────────────────────────────────────────

TEST(SimulationCore, DefaultGravityIsNegativeY) {
    RigidBodySolver s;
    SimVec3 g = s.gravity();
    EXPECT_FLOAT_EQ(g.x, 0.0f);
    EXPECT_LT(g.y, 0.0f);
    EXPECT_FLOAT_EQ(g.z, 0.0f);
}

TEST(SimulationCore, SetGravityChangesGravity) {
    RigidBodySolver s;
    s.setGravity({0.0f, -20.0f, 0.0f});
    SimVec3 g = s.gravity();
    EXPECT_FLOAT_EQ(g.y, -20.0f);
}

TEST(SimulationCore, ZeroGravityBodyFallsNoFurtherThanInitialVelocity) {
    RigidBodySolver s;
    s.setGravity({0.0f, 0.0f, 0.0f});

    SimBodyDesc desc;
    desc.mass     = 1.0f;
    desc.position = {0.0f, 0.0f, 0.0f};
    desc.velocity = {1.0f, 0.0f, 0.0f};
    BodyId id = s.addBody(desc);

    (void)s.step(1.0); // dt = 1 s, v = (1,0,0), no gravity, no forces
    SimVec3 pos, vel;
    ASSERT_TRUE(s.getBodyState(id, pos, vel));
    EXPECT_TRUE(vec3Eq(pos, {1.0f, 0.0f, 0.0f}));
    EXPECT_TRUE(vec3Eq(vel, {1.0f, 0.0f, 0.0f})); // no acceleration
}

// ── Simulation step ───────────────────────────────────────────────────────────

TEST(SimulationCore, StepRejectsZeroDt) {
    RigidBodySolver s;
    (void)s.addBody({1.0f});
    auto r = s.step(0.0);
    EXPECT_FALSE(r.ok);
    EXPECT_EQ(r.bodiesIntegrated, 0u);
    EXPECT_DOUBLE_EQ(s.simulationTime(), 0.0);
}

TEST(SimulationCore, StepRejectsNegativeDt) {
    RigidBodySolver s;
    (void)s.addBody({1.0f});
    auto r = s.step(-0.016);
    EXPECT_FALSE(r.ok);
    EXPECT_EQ(r.bodiesIntegrated, 0u);
}

TEST(SimulationCore, StepRejectsNonFiniteDt) {
    RigidBodySolver s;
    (void)s.addBody({1.0f});

    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();

    EXPECT_FALSE(s.step(nan).ok);
    EXPECT_FALSE(s.step(inf).ok);
    EXPECT_FALSE(s.step(-inf).ok);
}

TEST(SimulationCore, StepRejectsNonFiniteRuntimeState) {
    const double nan = std::numeric_limits<double>::quiet_NaN();

    {
        RigidBodySolver s;
        (void)s.addBody({1.0f});
        s.setGravity({static_cast<float>(nan), 0.0f, 0.0f});
        EXPECT_FALSE(s.step(0.016).ok);
    }

    {
        RigidBodySolver s;
        SimBodyDesc desc;
        desc.mass = 1.0f;
        desc.position = {static_cast<float>(nan), 0.0f, 0.0f};
        EXPECT_EQ(s.addBody(desc), kInvalidBodyId);
        EXPECT_EQ(s.bodyCount(), 0u);
    }

    {
        RigidBodySolver s;
        const BodyId id = s.addBody({1.0f});
        ASSERT_NE(id, kInvalidBodyId);
        EXPECT_FALSE(s.applyForce(id, {static_cast<float>(nan), 0.0f, 0.0f}));
        EXPECT_TRUE(s.step(0.016).ok);
    }
}

TEST(SimulationCore, StepAdvancesSimulationTime) {
    RigidBodySolver s;
    (void)s.step(0.016);
    EXPECT_DOUBLE_EQ(s.simulationTime(), 0.016);
}

TEST(SimulationCore, StepCountsOnlyDynamicBodies) {
    RigidBodySolver s;
    (void)s.addBody({1.0f}); // dynamic
    (void)s.addBody({0.0f}); // static
    auto r = s.step(0.016);
    EXPECT_EQ(r.bodiesIntegrated, 1u);
}

TEST(SimulationCore, StepFixedRejectsInvalidArguments) {
    RigidBodySolver s;
    (void)s.addBody({1.0f});

    const StepReport badDt = s.stepFixed(0.0, 0.01);
    EXPECT_FALSE(badDt.ok);

    const StepReport badSubstep = s.stepFixed(0.1, 0.0);
    EXPECT_FALSE(badSubstep.ok);
}

TEST(SimulationCore, StepFixedRejectsNonFiniteArguments) {
    RigidBodySolver s;
    (void)s.addBody({1.0f});

    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();

    EXPECT_FALSE(s.stepFixed(nan, 0.01).ok);
    EXPECT_FALSE(s.stepFixed(0.1, nan).ok);
    EXPECT_FALSE(s.stepFixed(inf, 0.01).ok);
    EXPECT_FALSE(s.stepFixed(0.1, inf).ok);
}

TEST(SimulationCore, StepFixedRejectsNonFiniteRuntimeState) {
    RigidBodySolver s;
    (void)s.addBody({1.0f});

    const float inf = std::numeric_limits<float>::infinity();
    s.setGravity({0.0f, -9.81f, inf});
    EXPECT_FALSE(s.stepFixed(0.1, 0.01).ok);
}

TEST(SimulationCore, StepFixedMatchesSingleStepWhenNoForcesAndNoGravity) {
    RigidBodySolver a;
    RigidBodySolver b;
    a.setGravity({0.0f, 0.0f, 0.0f});
    b.setGravity({0.0f, 0.0f, 0.0f});

    SimBodyDesc desc;
    desc.mass = 1.0f;
    desc.position = {1.0f, 2.0f, 3.0f};
    desc.velocity = {4.0f, -2.0f, 1.0f};

    const BodyId idA = a.addBody(desc);
    const BodyId idB = b.addBody(desc);

    ASSERT_TRUE(a.step(0.05).ok);
    ASSERT_TRUE(b.stepFixed(0.05, 0.01).ok);

    SimVec3 posA, velA;
    SimVec3 posB, velB;
    ASSERT_TRUE(a.getBodyState(idA, posA, velA));
    ASSERT_TRUE(b.getBodyState(idB, posB, velB));

    EXPECT_TRUE(vec3Eq(posA, posB, 1e-6f));
    EXPECT_TRUE(vec3Eq(velA, velB, 1e-6f));
    EXPECT_NEAR(a.simulationTime(), b.simulationTime(), 1e-12);
}

TEST(SimulationCore, StepFixedMixedScheduleIsDeterministicWhenAlignedToSubstep) {
    auto runSchedule = [](const std::vector<double>& schedule) {
        RigidBodySolver s;
        s.setGravity({0.0f, -9.81f, 0.0f});

        SimBodyDesc desc;
        desc.mass = 2.0f;
        desc.position = {0.0f, 10.0f, 0.0f};
        desc.velocity = {3.0f, 0.0f, 1.0f};
        const BodyId id = s.addBody(desc);

        for (const double dt : schedule) {
            const StepReport report = s.stepFixed(dt, 0.01);
            EXPECT_TRUE(report.ok);
        }

        SimVec3 pos, vel;
        EXPECT_TRUE(s.getBodyState(id, pos, vel));
        return std::pair<SimVec3, SimVec3>{pos, vel};
    };

    const auto runA = runSchedule({0.03, 0.07, 0.05, 0.05});
    const auto runB = runSchedule({0.10, 0.10});

    EXPECT_TRUE(vec3Eq(runA.first, runB.first, 1e-5f));
    EXPECT_TRUE(vec3Eq(runA.second, runB.second, 1e-5f));
}

TEST(SimulationCore, GravityAcceleratesBodyDownward) {
    RigidBodySolver s;
    s.setGravity({0.0f, -10.0f, 0.0f}); // g = 10 m/s² down

    SimBodyDesc desc;
    desc.mass     = 1.0f;
    desc.position = {0.0f, 100.0f, 0.0f};
    desc.velocity = {0.0f, 0.0f, 0.0f};
    BodyId id = s.addBody(desc);

    // dt = 1 s: a = (0,-10,0); v = (0,-10,0); p = (0,100,0) + (0,-10,0) = (0,90,0)
    (void)s.step(1.0);
    SimVec3 pos, vel;
    ASSERT_TRUE(s.getBodyState(id, pos, vel));
    EXPECT_TRUE(vec3Eq(vel, {0.0f, -10.0f, 0.0f}));
    EXPECT_TRUE(vec3Eq(pos, {0.0f, 90.0f, 0.0f}));
}

TEST(SimulationCore, AppliedForceClearedAfterStep) {
    RigidBodySolver s;
    s.setGravity({0.0f, 0.0f, 0.0f}); // no gravity

    BodyId id = s.addBody({1.0f, {0,0,0}, {0,0,0}});
    (void)s.applyForce(id, {10.0f, 0.0f, 0.0f}); // F = 10 N, a = 10 m/s²

    // Step 1: v becomes 10*dt, position moves
    (void)s.step(1.0);
    SimVec3 pos1, vel1;
    ASSERT_TRUE(s.getBodyState(id, pos1, vel1));
    EXPECT_FLOAT_EQ(vel1.x, 10.0f); // a=10, dt=1 → v=10

    // Step 2: no force applied → velocity should remain constant (no gravity, no force)
    (void)s.step(1.0);
    SimVec3 pos2, vel2;
    ASSERT_TRUE(s.getBodyState(id, pos2, vel2));
    EXPECT_FLOAT_EQ(vel2.x, 10.0f); // unchanged — force was cleared
}

TEST(SimulationCore, ApplyForceReturnsFalseForUnknownBody) {
    RigidBodySolver s;
    EXPECT_FALSE(s.applyForce(99u, {1.0f, 0.0f, 0.0f}));
}

// ── State capture and rollback ────────────────────────────────────────────────

TEST(SimulationCore, CaptureStatePreservesAllBodies) {
    RigidBodySolver s;
    BodyId a = s.addBody({1.0f, {1,2,3}, {4,5,6}});
    BodyId b = s.addBody({2.0f, {7,8,9}, {0,0,0}});

    SimState state = s.captureState();
    EXPECT_EQ(state.bodies.size(), 2u);
    EXPECT_DOUBLE_EQ(state.simulationTime, 0.0);

    bool foundA = false, foundB = false;
    for (const auto& snap : state.bodies) {
        if (snap.id == a) { foundA = true; EXPECT_TRUE(vec3Eq(snap.position, {1,2,3})); }
        if (snap.id == b) { foundB = true; EXPECT_TRUE(vec3Eq(snap.position, {7,8,9})); }
    }
    EXPECT_TRUE(foundA);
    EXPECT_TRUE(foundB);
}

TEST(SimulationCore, CaptureStatePreservesSimulationTime) {
    RigidBodySolver s;
    (void)s.addBody({1.0f});
    (void)s.step(0.5);
    SimState state = s.captureState();
    EXPECT_DOUBLE_EQ(state.simulationTime, 0.5);
}

TEST(SimulationCore, CaptureStateOrdersBodiesByAscendingId) {
    RigidBodySolver s;
    const BodyId idA = s.addBody({1.0f});
    const BodyId idB = s.addBody({1.0f});
    const BodyId idC = s.addBody({1.0f});
    ASSERT_TRUE(s.removeBody(idB));
    const BodyId idD = s.addBody({1.0f});

    SimState state = s.captureState();
    ASSERT_EQ(state.bodies.size(), 3u);

    EXPECT_EQ(state.bodies[0].id, idA);
    EXPECT_EQ(state.bodies[1].id, idC);
    EXPECT_EQ(state.bodies[2].id, idD);
}

TEST(SimulationCore, CaptureStateOrderIsDeterministicAcrossIdenticalSetup) {
    auto captureIds = []() {
        RigidBodySolver s;
        const BodyId idA = s.addBody({1.0f});
        const BodyId idB = s.addBody({1.0f});
        const BodyId idC = s.addBody({1.0f});
        (void)s.removeBody(idB);
        const BodyId idD = s.addBody({1.0f});

        SimState state = s.captureState();
        std::vector<BodyId> ids;
        ids.reserve(state.bodies.size());
        for (const SimBodySnapshot& body : state.bodies) {
            ids.push_back(body.id);
        }

        EXPECT_EQ(idA, 1u);
        EXPECT_EQ(idC, 3u);
        EXPECT_EQ(idD, 4u);
        return ids;
    };

    const std::vector<BodyId> a = captureIds();
    const std::vector<BodyId> b = captureIds();
    EXPECT_EQ(a, b);
}

TEST(SimulationCore, RestoreStateRollsBackPosition) {
    RigidBodySolver s;
    s.setGravity({0.0f, -10.0f, 0.0f});

    SimBodyDesc desc;
    desc.mass     = 1.0f;
    desc.position = {0.0f, 100.0f, 0.0f};
    BodyId id = s.addBody(desc);

    SimState snapshot = s.captureState(); // position = (0, 100, 0)

    (void)s.step(1.0); // falls to (0, 90, 0)
    (void)s.step(1.0); // falls further

    EXPECT_TRUE(s.restoreState(snapshot));

    SimVec3 pos, vel;
    ASSERT_TRUE(s.getBodyState(id, pos, vel));
    EXPECT_TRUE(vec3Eq(pos, {0.0f, 100.0f, 0.0f}));
    EXPECT_DOUBLE_EQ(s.simulationTime(), 0.0);
}

TEST(SimulationCore, RestoreStateAllowsIdenticalReplay) {
    // Same initial state + same steps must produce identical trajectories
    // (determinism + rollback correctness).
    RigidBodySolver s;
    s.setGravity({0.0f, -9.81f, 0.0f});

    SimBodyDesc desc;
    desc.mass     = 3.0f;
    desc.position = {5.0f, 50.0f, 2.0f};
    desc.velocity = {1.0f, 0.0f, 0.0f};
    BodyId id = s.addBody(desc);

    SimState snapshot = s.captureState();

    // First run: 10 steps of 0.016 s
    std::vector<SimVec3> firstRunPositions;
    for (int i = 0; i < 10; ++i) {
        (void)s.step(0.016);
        SimVec3 p, v;
        ASSERT_TRUE(s.getBodyState(id, p, v));
        firstRunPositions.push_back(p);
    }

    // Restore and replay
    ASSERT_TRUE(s.restoreState(snapshot));
    for (int i = 0; i < 10; ++i) {
        (void)s.step(0.016);
        SimVec3 p, v;
        ASSERT_TRUE(s.getBodyState(id, p, v));
        EXPECT_TRUE(vec3Eq(p, firstRunPositions[static_cast<std::size_t>(i)], 1e-5f))
            << "Mismatch at step " << i;
    }
}

TEST(SimulationCore, SimStateSerializationRoundTripIsDeterministic) {
    RigidBodySolver s;
    s.setGravity({0.0f, -9.81f, 0.0f});

    SimBodyDesc descA;
    descA.mass = 2.0f;
    descA.position = {1.0f, 2.0f, 3.0f};
    descA.velocity = {4.0f, 5.0f, 6.0f};

    SimBodyDesc descB;
    descB.mass = 1.0f;
    descB.position = {7.0f, 8.0f, 9.0f};
    descB.velocity = {0.5f, 0.25f, 0.125f};

    const BodyId bodyB = s.addBody(descB);
    const BodyId bodyA = s.addBody(descA);
    EXPECT_NE(bodyA, bodyB);

    (void)s.stepFixed(0.032, 0.016);

    const SimState snapshot = s.captureState();
    const std::vector<std::uint8_t> bytesA = serializeSimState(snapshot);
    const std::vector<std::uint8_t> bytesB = serializeSimState(snapshot);
    EXPECT_EQ(bytesA, bytesB);

    SimState restored;
    ASSERT_TRUE(deserializeSimState(bytesA, restored));
    EXPECT_EQ(restored, snapshot);

    const std::vector<std::uint8_t> bytesC = serializeSimState(restored);
    EXPECT_EQ(bytesA, bytesC);
}

TEST(SimulationCore, DeserializeSimStateRejectsMalformedBlob) {
    SimState restored;

    EXPECT_FALSE(deserializeSimState({}, restored));

    std::vector<std::uint8_t> malformed = {0x53u, 0x53u, 0x4du, 0x31u, 0x01u, 0x00u, 0x00u, 0x00u};
    EXPECT_FALSE(deserializeSimState(malformed, restored));
}

TEST(SimulationCore, RestoreStateReturnsFalseWhenSnapshotIsEmptyButSolverHasBodies) {
    RigidBodySolver s;
    (void)s.addBody({1.0f});
    SimState empty;
    EXPECT_FALSE(s.restoreState(empty));
}

TEST(SimulationCore, RestoreStateRejectsDuplicateBodyIds) {
    RigidBodySolver s;
    (void)s.addBody({1.0f});

    SimState invalid;
    invalid.bodies.push_back({1u, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}});
    invalid.bodies.push_back({1u, {1.0f, 2.0f, 3.0f}, {4.0f, 5.0f, 6.0f}});

    EXPECT_FALSE(s.restoreState(invalid));
}

TEST(SimulationCore, RestoreStateRejectsNonFiniteSnapshotData) {
    RigidBodySolver s;
    const BodyId id = s.addBody({1.0f, {1.0f, 2.0f, 3.0f}, {0.5f, 0.25f, -0.75f}});
    ASSERT_NE(id, kInvalidBodyId);

    const SimState baseline = s.captureState();

    SimState bad = baseline;
    bad.simulationTime = std::numeric_limits<double>::quiet_NaN();
    bad.bodies[0].position.x = std::numeric_limits<float>::infinity();
    bad.bodies[0].velocity.y = std::numeric_limits<float>::quiet_NaN();

    EXPECT_FALSE(s.restoreState(bad));

    const SimState after = s.captureState();
    EXPECT_EQ(after, baseline);
}

TEST(SimulationCore, SimStateSerializationOrdersExtremeBodyIdsDeterministically) {
    SimState state;
    state.simulationTime = 12.5;
    state.bodies.push_back({std::numeric_limits<BodyId>::max(), {1.0f, 2.0f, 3.0f}, {4.0f, 5.0f, 6.0f}});
    state.bodies.push_back({1u, {7.0f, 8.0f, 9.0f}, {0.5f, 0.25f, 0.125f}});
    state.bodies.push_back({std::numeric_limits<BodyId>::max() - 1u, {10.0f, 11.0f, 12.0f}, {13.0f, 14.0f, 15.0f}});

    const std::vector<std::uint8_t> bytesA = serializeSimState(state);
    const std::vector<std::uint8_t> bytesB = serializeSimState(state);
    EXPECT_EQ(bytesA, bytesB);

    SimState restored;
    ASSERT_TRUE(deserializeSimState(bytesA, restored));
    ASSERT_EQ(restored.bodies.size(), 3u);
    EXPECT_EQ(restored.bodies[0].id, 1u);
    EXPECT_EQ(restored.bodies[1].id, std::numeric_limits<BodyId>::max() - 1u);
    EXPECT_EQ(restored.bodies[2].id, std::numeric_limits<BodyId>::max());
}

TEST(SimulationCore, EmptySolverRestoresFromEmptyStateSuccessfully) {
    RigidBodySolver s;
    SimState empty;
    EXPECT_TRUE(s.restoreState(empty)); // both empty — ok
}

// ── Determinism ───────────────────────────────────────────────────────────────

TEST(SimulationCore, IdenticalSetupProducesIdenticalTrajectory) {
    auto run = []() {
        RigidBodySolver s;
        s.setGravity({0.0f, -9.81f, 0.0f});
        SimBodyDesc desc;
        desc.mass     = 2.0f;
        desc.position = {0.0f, 50.0f, 0.0f};
        desc.velocity = {3.0f, 5.0f, 1.0f};
        BodyId id = s.addBody(desc);
        for (int i = 0; i < 60; ++i) {
            (void)s.applyForce(id, {0.5f, 0.0f, 0.0f}); // constant thrust
            (void)s.step(0.016);
        }
        SimVec3 p, v;
        (void)s.getBodyState(id, p, v);
        return p;
    };

    SimVec3 a = run();
    SimVec3 b = run();
    EXPECT_TRUE(vec3Eq(a, b, 0.0f)); // bit-exact
}

TEST(SimulationCore, MultiBodyStepAllBodiesIntegrated) {
    RigidBodySolver s;
    s.setGravity({0.0f, 0.0f, 0.0f});
    const int N = 5;
    for (int i = 0; i < N; ++i) (void)s.addBody({1.0f});
    auto r = s.step(0.016);
    EXPECT_EQ(r.bodiesIntegrated, static_cast<std::size_t>(N));
}

TEST(SimulationCore, SimulationTimeAccumulatesAcrossMultipleSteps) {
    RigidBodySolver s;
    for (int i = 0; i < 10; ++i) (void)s.step(0.016);
    // 0.016f is not exactly representable; repeated float→double conversion
    // accumulates ~1e-8 error — use a tolerance that reflects this reality.
    EXPECT_NEAR(s.simulationTime(), 10.0 * 0.016, 1e-6);
}

// ── Angular dynamics ────────────────────────────────────────────────────────

static bool quatIsUnit(const SimQuat& q, float eps = 1e-4f) {
    const float lenSq = q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w;
    return std::fabs(lenSq - 1.0f) <= eps;
}

TEST(SimulationCore, DefaultBodyHasIdentityOrientationAndZeroAngularVelocity) {
    RigidBodySolver s;
    const BodyId id = s.addBody({1.0f});
    SimQuat q; SimVec3 w;
    ASSERT_TRUE(s.getBodyAngularState(id, q, w));
    EXPECT_EQ(q, (SimQuat{0.0f, 0.0f, 0.0f, 1.0f}));
    EXPECT_TRUE(vec3Eq(w, {0.0f, 0.0f, 0.0f}));
}

TEST(SimulationCore, AddBodyRejectsNonPositiveInertia) {
    RigidBodySolver s;
    // Each principal moment must be finite and > 0.
    SimBodyDesc zero;     zero.inertia     = {1.0f, 0.0f, 1.0f};
    SimBodyDesc negative; negative.inertia = {1.0f, 1.0f, -1.0f};
    SimBodyDesc nan;      nan.inertia      = {std::numeric_limits<float>::quiet_NaN(), 1.0f, 1.0f};
    EXPECT_EQ(s.addBody(zero), kInvalidBodyId);
    EXPECT_EQ(s.addBody(negative), kInvalidBodyId);
    EXPECT_EQ(s.addBody(nan), kInvalidBodyId);
}

TEST(SimulationCore, AddBodyRejectsNonFiniteAngularState) {
    RigidBodySolver s;
    SimBodyDesc badOrient;
    badOrient.orientation.x = std::numeric_limits<float>::infinity();
    SimBodyDesc badAngVel;
    badAngVel.angularVelocity.z = std::numeric_limits<float>::quiet_NaN();
    EXPECT_EQ(s.addBody(badOrient), kInvalidBodyId);
    EXPECT_EQ(s.addBody(badAngVel), kInvalidBodyId);
}

TEST(SimulationCore, ConstantAngularVelocitySpinsOrientationDeterministically) {
    auto run = []() {
        RigidBodySolver s;
        s.setGravity({0.0f, 0.0f, 0.0f});
        SimBodyDesc desc;
        desc.mass = 1.0f;
        desc.angularVelocity = {0.0f, 1.0f, 0.0f}; // 1 rad/s about +Y
        const BodyId id = s.addBody(desc);
        for (int i = 0; i < 16; ++i) (void)s.step(0.05);
        SimQuat q; SimVec3 w;
        EXPECT_TRUE(s.getBodyAngularState(id, q, w));
        return std::pair{q, w};
    };

    const auto [q, w] = run();
    EXPECT_TRUE(quatIsUnit(q));            // renormalized each step
    EXPECT_GT(q.y, 0.0f);                  // rotated about +Y
    EXPECT_TRUE(approxEq(q.x, 0.0f));
    EXPECT_TRUE(approxEq(q.z, 0.0f));
    EXPECT_TRUE(vec3Eq(w, {0.0f, 1.0f, 0.0f})); // no torque → angular velocity preserved

    const auto [q2, w2] = run();
    EXPECT_EQ(q, q2);                      // bit-identical determinism
    EXPECT_EQ(w, w2);
}

TEST(SimulationCore, TorqueAcceleratesAngularVelocityThenClears) {
    RigidBodySolver s;
    s.setGravity({0.0f, 0.0f, 0.0f});
    SimBodyDesc desc;
    desc.mass = 1.0f;
    desc.inertia = {2.0f, 2.0f, 2.0f};
    const BodyId id = s.addBody(desc);

    ASSERT_TRUE(s.applyTorque(id, {0.0f, 4.0f, 0.0f})); // angAccel = 4/2 = 2 rad/s^2
    (void)s.step(0.5);                                  // angVel.y = 2 * 0.5 = 1.0

    SimQuat q; SimVec3 w;
    ASSERT_TRUE(s.getBodyAngularState(id, q, w));
    EXPECT_TRUE(approxEq(w.y, 1.0f));

    (void)s.step(0.5); // torque was cleared → angular velocity stays constant
    ASSERT_TRUE(s.getBodyAngularState(id, q, w));
    EXPECT_TRUE(approxEq(w.y, 1.0f));
}

TEST(SimulationCore, ApplyTorqueRejectsStaticUnknownAndNonFinite) {
    RigidBodySolver s;
    const BodyId staticId = s.addBody({0.0f});           // mass 0 → static
    const BodyId dynId    = s.addBody({1.0f});
    EXPECT_FALSE(s.applyTorque(staticId, {0.0f, 1.0f, 0.0f}));
    EXPECT_FALSE(s.applyTorque(9999u, {0.0f, 1.0f, 0.0f}));
    EXPECT_FALSE(s.applyTorque(dynId, {std::numeric_limits<float>::infinity(), 0.0f, 0.0f}));
    EXPECT_TRUE(s.applyTorque(dynId, {0.0f, 1.0f, 0.0f}));
}

TEST(SimulationCore, StaticBodyOrientationUnchangedAfterStep) {
    RigidBodySolver s;
    s.setGravity({0.0f, 0.0f, 0.0f});
    SimBodyDesc desc;
    desc.mass = 0.0f;                       // static
    desc.angularVelocity = {0.0f, 5.0f, 0.0f};
    const BodyId id = s.addBody(desc);
    (void)s.step(0.1);
    SimQuat q; SimVec3 w;
    ASSERT_TRUE(s.getBodyAngularState(id, q, w));
    EXPECT_EQ(q, (SimQuat{0.0f, 0.0f, 0.0f, 1.0f})); // identity, not integrated
}

TEST(SimulationCore, CaptureStatePreservesAngularState) {
    RigidBodySolver s;
    s.setGravity({0.0f, 0.0f, 0.0f});
    SimBodyDesc desc;
    desc.mass = 1.0f;
    desc.orientation = {0.0f, 0.70710678f, 0.0f, 0.70710678f}; // 90° about Y
    desc.angularVelocity = {0.0f, 2.0f, 0.0f};
    const BodyId id = s.addBody(desc);

    const SimState state = s.captureState();
    ASSERT_EQ(state.bodies.size(), 1u);
    EXPECT_EQ(state.bodies[0].id, id);
    EXPECT_TRUE(approxEq(state.bodies[0].orientation.y, 0.70710678f));
    EXPECT_TRUE(approxEq(state.bodies[0].orientation.w, 0.70710678f));
    EXPECT_TRUE(vec3Eq(state.bodies[0].angularVelocity, {0.0f, 2.0f, 0.0f}));
}

TEST(SimulationCore, SerializationRoundTripPreservesAngularState) {
    SimState state;
    state.simulationTime = 3.25;
    SimBodySnapshot snap{};
    snap.id = 7u;
    snap.position = {1.0f, 2.0f, 3.0f};
    snap.velocity = {0.1f, 0.2f, 0.3f};
    snap.orientation = {0.0f, 0.70710678f, 0.0f, 0.70710678f};
    snap.angularVelocity = {0.0f, 1.5f, 0.0f};
    state.bodies.push_back(snap);

    const std::vector<std::uint8_t> bytes = serializeSimState(state);
    SimState restored;
    ASSERT_TRUE(deserializeSimState(bytes, restored));
    EXPECT_EQ(restored, state);
}

TEST(SimulationCore, DeserializeAcceptsLegacyV1Blob) {
    // Hand-build a version-1 blob (no angular fields) and verify it decodes with
    // identity orientation + zero angular velocity for forward compatibility.
    auto appendU32 = [](std::vector<std::uint8_t>& v, std::uint32_t x) {
        for (int i = 0; i < 4; ++i) v.push_back(static_cast<std::uint8_t>((x >> (8 * i)) & 0xFFu));
    };
    auto appendU64 = [](std::vector<std::uint8_t>& v, std::uint64_t x) {
        for (int i = 0; i < 8; ++i) v.push_back(static_cast<std::uint8_t>((x >> (8 * i)) & 0xFFu));
    };
    auto appendF = [&](std::vector<std::uint8_t>& v, float f) {
        appendU32(v, std::bit_cast<std::uint32_t>(f));
    };

    std::vector<std::uint8_t> blob;
    appendU32(blob, 0x314d5353u);                                  // magic 'SSM1'
    appendU32(blob, 1u);                                           // version 1
    appendU64(blob, std::bit_cast<std::uint64_t>(2.5));            // simulation time
    appendU32(blob, 1u);                                           // body count
    appendU32(blob, 42u);                                          // body id
    appendF(blob, 1.0f); appendF(blob, 2.0f); appendF(blob, 3.0f); // position
    appendF(blob, 4.0f); appendF(blob, 5.0f); appendF(blob, 6.0f); // velocity

    SimState restored;
    ASSERT_TRUE(deserializeSimState(blob, restored));
    ASSERT_EQ(restored.bodies.size(), 1u);
    EXPECT_DOUBLE_EQ(restored.simulationTime, 2.5);
    EXPECT_EQ(restored.bodies[0].id, 42u);
    EXPECT_TRUE(vec3Eq(restored.bodies[0].position, {1.0f, 2.0f, 3.0f}));
    EXPECT_EQ(restored.bodies[0].orientation, (SimQuat{0.0f, 0.0f, 0.0f, 1.0f}));
    EXPECT_TRUE(vec3Eq(restored.bodies[0].angularVelocity, {0.0f, 0.0f, 0.0f}));
}

TEST(SimulationCore, RestoreStateRejectsNonFiniteAngularState) {
    RigidBodySolver s;
    (void)s.addBody({1.0f});
    const SimState baseline = s.captureState();

    SimState bad = baseline;
    bad.bodies[0].orientation.x = std::numeric_limits<float>::infinity();
    EXPECT_FALSE(s.restoreState(bad));

    SimState bad2 = baseline;
    bad2.bodies[0].angularVelocity.z = std::numeric_limits<float>::quiet_NaN();
    EXPECT_FALSE(s.restoreState(bad2));

    EXPECT_EQ(s.captureState(), baseline);
}

// ── Damping ─────────────────────────────────────────────────────────────────

TEST(SimulationCore, LinearDampingDecaysVelocity) {
    RigidBodySolver s;
    s.setGravity({0.0f, 0.0f, 0.0f});
    SimBodyDesc d;
    d.mass = 1.0f;
    d.velocity = {10.0f, 0.0f, 0.0f};
    d.linearDamping = 2.0f;                // factor = 1 - 2*0.1 = 0.8
    const BodyId id = s.addBody(d);

    (void)s.step(0.1);
    SimVec3 pos, vel;
    ASSERT_TRUE(s.getBodyState(id, pos, vel));
    EXPECT_TRUE(approxEq(vel.x, 8.0f));    // 10 * 0.8
    EXPECT_TRUE(approxEq(pos.x, 0.8f));    // damped velocity integrated
}

TEST(SimulationCore, AngularDampingDecaysAngularVelocity) {
    RigidBodySolver s;
    s.setGravity({0.0f, 0.0f, 0.0f});
    SimBodyDesc d;
    d.mass = 1.0f;
    d.angularVelocity = {0.0f, 4.0f, 0.0f};
    d.angularDamping = 1.0f;               // factor = 1 - 1*0.25 = 0.75
    const BodyId id = s.addBody(d);

    (void)s.step(0.25);
    SimQuat q; SimVec3 w;
    ASSERT_TRUE(s.getBodyAngularState(id, q, w));
    EXPECT_TRUE(approxEq(w.y, 3.0f));      // 4 * 0.75
}

TEST(SimulationCore, ZeroDampingPreservesVelocity) {
    RigidBodySolver s;
    s.setGravity({0.0f, 0.0f, 0.0f});
    SimBodyDesc d;
    d.mass = 1.0f;
    d.velocity = {5.0f, 0.0f, 0.0f};       // default damping 0
    const BodyId id = s.addBody(d);

    (void)s.step(0.1);
    SimVec3 pos, vel;
    ASSERT_TRUE(s.getBodyState(id, pos, vel));
    EXPECT_TRUE(approxEq(vel.x, 5.0f));    // unchanged
}

TEST(SimulationCore, HighDampingClampsVelocityToZeroNotNegative) {
    RigidBodySolver s;
    s.setGravity({0.0f, 0.0f, 0.0f});
    SimBodyDesc d;
    d.mass = 1.0f;
    d.velocity = {10.0f, 0.0f, 0.0f};
    d.linearDamping = 100.0f;              // 1 - 100*0.1 = -9 -> clamp to 0
    const BodyId id = s.addBody(d);

    (void)s.step(0.1);
    SimVec3 pos, vel;
    ASSERT_TRUE(s.getBodyState(id, pos, vel));
    EXPECT_TRUE(approxEq(vel.x, 0.0f));    // killed, never reversed
}

TEST(SimulationCore, AddBodyRejectsNegativeOrNonFiniteDamping) {
    RigidBodySolver s;
    SimBodyDesc negLinear;  negLinear.linearDamping  = -1.0f;
    SimBodyDesc negAngular; negAngular.angularDamping = -0.5f;
    SimBodyDesc nanLinear;  nanLinear.linearDamping  = std::numeric_limits<float>::quiet_NaN();
    EXPECT_EQ(s.addBody(negLinear),  kInvalidBodyId);
    EXPECT_EQ(s.addBody(negAngular), kInvalidBodyId);
    EXPECT_EQ(s.addBody(nanLinear),  kInvalidBodyId);
}

TEST(SimulationCore, StepFixedAppliesDampingPerSubstep) {
    RigidBodySolver s;
    s.setGravity({0.0f, 0.0f, 0.0f});
    SimBodyDesc d;
    d.mass = 1.0f;
    d.velocity = {10.0f, 0.0f, 0.0f};
    d.linearDamping = 1.0f;
    const BodyId id = s.addBody(d);

    // Two 0.1 substeps: 10 * 0.9 * 0.9 = 8.1 (damping compounds per substep).
    (void)s.stepFixed(0.2, 0.1);
    SimVec3 pos, vel;
    ASSERT_TRUE(s.getBodyState(id, pos, vel));
    EXPECT_TRUE(approxEq(vel.x, 8.1f));
}

TEST(SimulationCore, DampedTrajectoryIsDeterministic) {
    auto run = []() {
        RigidBodySolver s;
        s.setGravity({0.0f, -9.81f, 0.0f});
        SimBodyDesc d;
        d.mass = 2.0f;
        d.velocity = {3.0f, 1.0f, -2.0f};
        d.angularVelocity = {0.5f, 1.0f, 0.0f};
        d.linearDamping  = 0.4f;
        d.angularDamping = 0.7f;
        const BodyId id = s.addBody(d);
        for (int i = 0; i < 32; ++i) (void)s.step(0.016);
        SimVec3 pos, vel; (void)s.getBodyState(id, pos, vel);
        SimQuat q; SimVec3 w; (void)s.getBodyAngularState(id, q, w);
        return std::tuple{pos, vel, q, w};
    };
    EXPECT_TRUE(run() == run());
}

// ── Inertia tensor (anisotropic rotation) ───────────────────────────────────

TEST(SimulationCore, IsotropicTensorMatchesScalarTorqueResponse) {
    RigidBodySolver s;
    s.setGravity({0.0f, 0.0f, 0.0f});
    SimBodyDesc d;
    d.mass = 1.0f;
    d.inertia = {4.0f, 4.0f, 4.0f};        // isotropic -> behaves like scalar I = 4
    const BodyId id = s.addBody(d);

    ASSERT_TRUE(s.applyTorque(id, {8.0f, 0.0f, 0.0f}));
    (void)s.step(0.25);                    // omega.x = (8/4) * 0.25 = 0.5
    SimQuat q; SimVec3 w;
    ASSERT_TRUE(s.getBodyAngularState(id, q, w));
    EXPECT_TRUE(approxEq(w.x, 0.5f));
    EXPECT_TRUE(approxEq(w.y, 0.0f));
    EXPECT_TRUE(approxEq(w.z, 0.0f));
}

TEST(SimulationCore, PrincipalAxisSpinStaysOnAxis) {
    RigidBodySolver s;
    s.setGravity({0.0f, 0.0f, 0.0f});
    SimBodyDesc d;
    d.mass = 1.0f;
    d.inertia = {1.0f, 2.0f, 3.0f};        // anisotropic
    d.angularVelocity = {0.0f, 5.0f, 0.0f}; // pure spin about a principal (Y) axis
    const BodyId id = s.addBody(d);

    for (int i = 0; i < 200; ++i) (void)s.step(0.01);

    SimQuat q; SimVec3 w;
    ASSERT_TRUE(s.getBodyAngularState(id, q, w));
    EXPECT_TRUE(vec3Eq(w, {0.0f, 5.0f, 0.0f}, 1e-2f)); // stable: no spurious precession
}

TEST(SimulationCore, AnisotropicFreeBodyPrecesses) {
    RigidBodySolver s;
    s.setGravity({0.0f, 0.0f, 0.0f});
    SimBodyDesc d;
    d.mass = 1.0f;
    d.inertia = {1.0f, 2.0f, 3.0f};
    d.angularVelocity = {2.0f, 1.0f, 0.5f}; // off principal axis -> tumbles
    const BodyId id = s.addBody(d);

    const SimVec3 w0 = d.angularVelocity;
    for (int i = 0; i < 150; ++i) (void)s.step(0.02);

    SimQuat q; SimVec3 w;
    ASSERT_TRUE(s.getBodyAngularState(id, q, w));
    // World angular velocity must change for a freely tumbling asymmetric body.
    EXPECT_FALSE(vec3Eq(w, w0, 1e-2f));
}

TEST(SimulationCore, AnisotropicSpinStaysBoundedOverManySteps) {
    RigidBodySolver s;
    s.setGravity({0.0f, 0.0f, 0.0f});
    SimBodyDesc d;
    d.mass = 1.0f;
    d.inertia = {1.0f, 2.0f, 3.0f};
    d.angularVelocity = {2.0f, 1.0f, 0.5f};
    const BodyId id = s.addBody(d);

    for (int i = 0; i < 2000; ++i) (void)s.step(0.01);

    SimQuat q; SimVec3 w;
    ASSERT_TRUE(s.getBodyAngularState(id, q, w));
    const float mag = std::sqrt(w.x*w.x + w.y*w.y + w.z*w.z);
    // |w| <= |L|/min(I); a naive explicit gyroscopic integrator would diverge here.
    EXPECT_LT(mag, 5.0f);
    EXPECT_GT(mag, 0.1f); // and it does not collapse to zero
}

TEST(SimulationCore, AngularMomentumMagnitudeConservedUnderZeroTorque) {
    RigidBodySolver s;
    s.setGravity({0.0f, 0.0f, 0.0f});
    SimBodyDesc d;
    d.mass = 1.0f;
    d.inertia = {1.0f, 2.0f, 3.0f};
    d.angularVelocity = {2.0f, 1.0f, 0.5f};
    const BodyId id = s.addBody(d);

    auto cross = [](const SimVec3& a, const SimVec3& b) {
        return SimVec3{a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x};
    };
    auto invRot = [&](const SimQuat& q, const SimVec3& v) {
        const SimVec3 u{-q.x, -q.y, -q.z};
        const SimVec3 t = cross(u, v) * 2.0f;
        return v + t * q.w + cross(u, t);
    };
    const SimVec3 I = d.inertia;
    auto momentumMag = [&]() {
        SimQuat q; SimVec3 w;
        (void)s.getBodyAngularState(id, q, w);
        const SimVec3 wBody = invRot(q, w);             // omega in body frame
        const SimVec3 lBody{wBody.x*I.x, wBody.y*I.y, wBody.z*I.z};
        return std::sqrt(lBody.x*lBody.x + lBody.y*lBody.y + lBody.z*lBody.z);
    };

    const float l0 = momentumMag();
    for (int i = 0; i < 300; ++i) (void)s.step(0.01);
    const float l1 = momentumMag();
    EXPECT_NEAR(l1, l0, l0 * 0.02f); // |L| conserved (world rotation preserves its norm)
}

// ── Ground-plane collision ──────────────────────────────────────────────────────

TEST(SimulationCore, GroundPlaneDisabledByDefault) {
    RigidBodySolver s;
    EXPECT_FALSE(s.hasGroundPlane());
}

TEST(SimulationCore, SetAndClearGroundPlaneToggleState) {
    RigidBodySolver s;
    s.setGroundPlane({0.0f, 1.0f, 0.0f}, 0.0f, 0.5f);
    EXPECT_TRUE(s.hasGroundPlane());
    s.clearGroundPlane();
    EXPECT_FALSE(s.hasGroundPlane());
}

TEST(SimulationCore, SetGroundPlaneRejectsNonFiniteAndDegenerateNormal) {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    RigidBodySolver s;
    s.setGroundPlane({nan, 1.0f, 0.0f}, 0.0f, 0.5f);   // non-finite normal
    EXPECT_FALSE(s.hasGroundPlane());
    s.setGroundPlane({0.0f, 1.0f, 0.0f}, inf, 0.5f);   // non-finite offset
    EXPECT_FALSE(s.hasGroundPlane());
    s.setGroundPlane({0.0f, 0.0f, 0.0f}, 0.0f, 0.5f);  // degenerate (zero) normal
    EXPECT_FALSE(s.hasGroundPlane());
}

TEST(SimulationCore, WithoutGroundPlaneBodyFallsThrough) {
    RigidBodySolver s;
    const BodyId id = s.addBody({.mass = 1.0f, .position = {0.0f, 1.0f, 0.0f}, .collisionRadius = 0.5f});
    for (int i = 0; i < 100; ++i) (void)s.step(0.01);
    SimVec3 pos, vel;
    ASSERT_TRUE(s.getBodyState(id, pos, vel));
    EXPECT_LT(pos.y, 0.0f);   // no collision opted in -> passes through y=0
}

TEST(SimulationCore, BodyRestsOnGroundInelastic) {
    RigidBodySolver s;
    // Non-unit normal exercises internal normalization; plane y = 0.
    s.setGroundPlane({0.0f, 5.0f, 0.0f}, 0.0f, 0.0f);
    const BodyId id = s.addBody({.mass = 1.0f, .position = {0.0f, 3.0f, 0.0f}, .collisionRadius = 0.5f});
    for (int i = 0; i < 400; ++i) (void)s.step(0.01);
    SimVec3 pos, vel;
    ASSERT_TRUE(s.getBodyState(id, pos, vel));
    EXPECT_NEAR(pos.y, 0.5f, 1e-3f);   // rests tangent to the plane, never penetrates
    EXPECT_NEAR(vel.y, 0.0f, 1e-3f);   // inelastic -> normal velocity removed
}

TEST(SimulationCore, ElasticBounceReversesNormalVelocity) {
    RigidBodySolver s;
    s.setGravity({0.0f, 0.0f, 0.0f});                  // isolate the bounce
    s.setGroundPlane({0.0f, 1.0f, 0.0f}, 0.0f, 1.0f);  // perfectly elastic
    const BodyId id = s.addBody({.mass = 1.0f,
                                 .position = {0.0f, 0.5f, 0.0f},   // resting on the surface
                                 .velocity = {0.0f, -2.0f, 0.0f},  // moving into the plane
                                 .collisionRadius = 0.5f});
    (void)s.step(0.01);
    SimVec3 pos, vel;
    ASSERT_TRUE(s.getBodyState(id, pos, vel));
    EXPECT_NEAR(vel.y, 2.0f, 1e-4f);   // inbound normal velocity reflected
    EXPECT_NEAR(pos.y, 0.5f, 1e-4f);   // pushed back tangent to the plane
}

TEST(SimulationCore, PenetratingBodyIsPushedOut) {
    RigidBodySolver s;
    s.setGravity({0.0f, 0.0f, 0.0f});
    s.setGroundPlane({0.0f, 1.0f, 0.0f}, 0.0f, 0.0f);
    // Center exactly on the plane -> fully penetrating by the radius.
    const BodyId id = s.addBody({.mass = 1.0f, .position = {0.0f, 0.0f, 0.0f}, .collisionRadius = 0.5f});
    (void)s.step(0.01);
    SimVec3 pos, vel;
    ASSERT_TRUE(s.getBodyState(id, pos, vel));
    EXPECT_NEAR(pos.y, 0.5f, 1e-4f);
}

TEST(SimulationCore, RadiusZeroBodyIgnoresGround) {
    RigidBodySolver s;
    s.setGroundPlane({0.0f, 1.0f, 0.0f}, 0.0f, 0.5f);
    const BodyId id = s.addBody({.mass = 1.0f, .position = {0.0f, 1.0f, 0.0f}}); // no collider
    for (int i = 0; i < 100; ++i) (void)s.step(0.01);
    SimVec3 pos, vel;
    ASSERT_TRUE(s.getBodyState(id, pos, vel));
    EXPECT_LT(pos.y, 0.0f);   // radius 0 -> no collision
}

TEST(SimulationCore, NonAxisAlignedPlaneBounces) {
    RigidBodySolver s;
    s.setGravity({0.0f, 0.0f, 0.0f});
    s.setGroundPlane({1.0f, 0.0f, 0.0f}, 0.0f, 1.0f);  // plane x = 0
    const BodyId id = s.addBody({.mass = 1.0f,
                                 .position = {0.5f, 0.0f, 0.0f},   // resting on the surface
                                 .velocity = {-3.0f, 0.0f, 0.0f},  // moving into the plane
                                 .collisionRadius = 0.5f});
    (void)s.step(0.01);
    SimVec3 pos, vel;
    ASSERT_TRUE(s.getBodyState(id, pos, vel));
    EXPECT_NEAR(vel.x, 3.0f, 1e-4f);   // reflected along the plane normal
    EXPECT_NEAR(pos.x, 0.5f, 1e-4f);
}

// ── Body-body (sphere-sphere) collision ─────────────────────────────────────────

TEST(SimulationCore, BodyCollisionDisabledByDefault) {
    RigidBodySolver s;
    EXPECT_FALSE(s.hasBodyCollision());
}

TEST(SimulationCore, SetAndClearBodyCollisionToggleState) {
    RigidBodySolver s;
    s.setBodyCollision(0.5f);
    EXPECT_TRUE(s.hasBodyCollision());
    s.clearBodyCollision();
    EXPECT_FALSE(s.hasBodyCollision());
}

TEST(SimulationCore, SetBodyCollisionRejectsNonFinite) {
    RigidBodySolver s;
    s.setBodyCollision(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FALSE(s.hasBodyCollision());
}

TEST(SimulationCore, EqualMassHeadOnElasticSwapsVelocities) {
    RigidBodySolver s;
    s.setGravity({0.0f, 0.0f, 0.0f});
    s.setBodyCollision(1.0f); // perfectly elastic
    const BodyId a = s.addBody({.mass = 1.0f, .position = {-0.4f, 0.0f, 0.0f},
                                .velocity = {2.0f, 0.0f, 0.0f}, .collisionRadius = 0.5f});
    const BodyId b = s.addBody({.mass = 1.0f, .position = {0.4f, 0.0f, 0.0f},
                                .velocity = {-2.0f, 0.0f, 0.0f}, .collisionRadius = 0.5f});
    (void)s.step(0.01);

    SimVec3 pa, va, pb, vb;
    ASSERT_TRUE(s.getBodyState(a, pa, va));
    ASSERT_TRUE(s.getBodyState(b, pb, vb));
    // Equal-mass perfectly-elastic head-on collision swaps the velocities.
    EXPECT_NEAR(va.x, -2.0f, 1e-4f);
    EXPECT_NEAR(vb.x,  2.0f, 1e-4f);
    EXPECT_LT(pa.x, pb.x); // separated, A still left of B
}

TEST(SimulationCore, StaticBodyActsImmovable) {
    RigidBodySolver s;
    s.setGravity({0.0f, 0.0f, 0.0f});
    s.setBodyCollision(1.0f);
    // A is static (mass 0); B approaches it from +x moving -x.
    (void)s.addBody({.mass = 0.0f, .position = {0.0f, 0.0f, 0.0f}, .collisionRadius = 0.5f});
    const BodyId b = s.addBody({.mass = 1.0f, .position = {0.8f, 0.0f, 0.0f},
                                .velocity = {-2.0f, 0.0f, 0.0f}, .collisionRadius = 0.5f});
    (void)s.step(0.01);

    SimVec3 pb, vb;
    ASSERT_TRUE(s.getBodyState(b, pb, vb));
    EXPECT_NEAR(vb.x, 2.0f, 1e-4f);   // bounces off the immovable body
    EXPECT_GE(pb.x, 0.5f);            // pushed clear (centers >= rSum apart)
}

TEST(SimulationCore, HeavierBodyMovesLessOnSeparation) {
    RigidBodySolver s;
    s.setGravity({0.0f, 0.0f, 0.0f});
    s.setBodyCollision(0.0f);
    const BodyId heavy = s.addBody({.mass = 10.0f, .position = {-0.4f, 0.0f, 0.0f}, .collisionRadius = 0.5f});
    const BodyId light = s.addBody({.mass = 1.0f,  .position = {0.4f, 0.0f, 0.0f},  .collisionRadius = 0.5f});
    (void)s.step(0.01);

    SimVec3 ph, vh, pl, vl;
    ASSERT_TRUE(s.getBodyState(heavy, ph, vh));
    ASSERT_TRUE(s.getBodyState(light, pl, vl));
    const float movedHeavy = std::fabs(ph.x - (-0.4f));
    const float movedLight = std::fabs(pl.x - (0.4f));
    EXPECT_GT(movedLight, movedHeavy);          // lighter body absorbs most of the push-out
    EXPECT_NEAR(movedLight, movedHeavy * 10.0f, movedHeavy * 0.5f); // ~inverse-mass ratio
}

TEST(SimulationCore, SeparatedBodiesDoNotInteract) {
    RigidBodySolver s;
    s.setGravity({0.0f, 0.0f, 0.0f});
    s.setBodyCollision(1.0f);
    const BodyId a = s.addBody({.mass = 1.0f, .position = {-5.0f, 0.0f, 0.0f}, .collisionRadius = 0.5f});
    const BodyId b = s.addBody({.mass = 1.0f, .position = {5.0f, 0.0f, 0.0f},  .collisionRadius = 0.5f});
    (void)s.step(0.01);

    SimVec3 pa, va, pb, vb;
    ASSERT_TRUE(s.getBodyState(a, pa, va));
    ASSERT_TRUE(s.getBodyState(b, pb, vb));
    EXPECT_NEAR(pa.x, -5.0f, 1e-5f);
    EXPECT_NEAR(pb.x,  5.0f, 1e-5f);
}

TEST(SimulationCore, BodyCollisionIsDeterministic) {
    auto run = [] {
        RigidBodySolver s;
        s.setGravity({0.0f, -9.81f, 0.0f});
        s.setBodyCollision(0.5f);
        (void)s.addBody({.mass = 1.0f, .position = {-0.3f, 1.0f, 0.0f},
                         .velocity = {1.0f, 0.0f, 0.0f}, .collisionRadius = 0.5f});
        (void)s.addBody({.mass = 2.0f, .position = {0.3f, 1.0f, 0.0f},
                         .velocity = {-1.0f, 0.0f, 0.0f}, .collisionRadius = 0.5f});
        for (int i = 0; i < 50; ++i) (void)s.step(0.01);
        return serializeSimState(s.captureState());
    };
    EXPECT_EQ(run(), run()); // identical inputs -> identical trajectory
}

// ── Broadphase (sweep-and-prune) ────────────────────────────────────────────────

TEST(SimulationCore, BroadphaseResolvesMultipleOverlapsAndPrunesDistant) {
    RigidBodySolver s;
    s.setGravity({0.0f, 0.0f, 0.0f});
    s.setBodyCollision(0.0f);
    // Three unit-radius spheres in a row: A and B overlap, B and C overlap (a chain
    // that exercises the sweep across more than one pair). A fourth sphere sits far
    // away and must be pruned (and so left untouched).
    const BodyId a = s.addBody({.mass = 1.0f, .position = {0.0f, 0.0f, 0.0f}, .collisionRadius = 0.5f});
    const BodyId b = s.addBody({.mass = 1.0f, .position = {0.6f, 0.0f, 0.0f}, .collisionRadius = 0.5f});
    const BodyId c = s.addBody({.mass = 1.0f, .position = {1.2f, 0.0f, 0.0f}, .collisionRadius = 0.5f});
    const BodyId far = s.addBody({.mass = 1.0f, .position = {50.0f, 0.0f, 0.0f}, .collisionRadius = 0.5f});

    for (int i = 0; i < 200; ++i) (void)s.step(0.01);

    SimVec3 pa, pb, pc, pf, v;
    ASSERT_TRUE(s.getBodyState(a, pa, v));
    ASSERT_TRUE(s.getBodyState(b, pb, v));
    ASSERT_TRUE(s.getBodyState(c, pc, v));
    ASSERT_TRUE(s.getBodyState(far, pf, v));

    // Every adjacent pair ends up at least ~2r apart (no residual interpenetration).
    EXPECT_GE(std::fabs(pb.x - pa.x), 1.0f - 1e-2f);
    EXPECT_GE(std::fabs(pc.x - pb.x), 1.0f - 1e-2f);
    // The distant body was pruned by the sweep and never moved.
    EXPECT_NEAR(pf.x, 50.0f, 1e-4f);
}

// ── Contact solver iterations ───────────────────────────────────────────────────

TEST(SimulationCore, SolverIterationsDefaultsToOneAndClampsLow) {
    RigidBodySolver s;
    EXPECT_EQ(s.solverIterations(), 1u);
    s.setSolverIterations(8u);
    EXPECT_EQ(s.solverIterations(), 8u);
    s.setSolverIterations(0u);          // clamped up to 1
    EXPECT_EQ(s.solverIterations(), 1u);
}

TEST(SimulationCore, StackSettlesWithoutInterpenetration) {
    RigidBodySolver s;
    s.setGravity({0.0f, -9.81f, 0.0f});
    s.setGroundPlane({0.0f, 1.0f, 0.0f}, 0.0f, 0.0f); // inelastic floor
    s.setBodyCollision(0.0f);                          // inelastic contacts
    s.setSolverIterations(16u);                        // enough passes to settle the stack

    constexpr float r = 0.5f;
    // Three spheres stacked and just touching; gravity compresses them, the iterated
    // solver pushes them back apart against the floor.
    const BodyId b0 = s.addBody({.mass = 1.0f, .position = {0.0f, 0.5f, 0.0f}, .collisionRadius = r});
    const BodyId b1 = s.addBody({.mass = 1.0f, .position = {0.0f, 1.5f, 0.0f}, .collisionRadius = r});
    const BodyId b2 = s.addBody({.mass = 1.0f, .position = {0.0f, 2.5f, 0.0f}, .collisionRadius = r});

    for (int i = 0; i < 600; ++i) (void)s.step(0.01);

    SimVec3 p0, p1, p2, v;
    ASSERT_TRUE(s.getBodyState(b0, p0, v));
    ASSERT_TRUE(s.getBodyState(b1, p1, v));
    ASSERT_TRUE(s.getBodyState(b2, p2, v));

    // Ordering preserved and no significant interpenetration: the bottom rests on
    // the floor and each gap stays near one diameter.
    const float tol = 0.05f;
    EXPECT_GE(p0.y, r - tol);                  // bottom on/above the floor
    EXPECT_GE(p1.y - p0.y, 2.0f * r - tol);    // gap b0->b1 ~ diameter
    EXPECT_GE(p2.y - p1.y, 2.0f * r - tol);    // gap b1->b2 ~ diameter
    EXPECT_LT(p0.y, 1.0f);                     // didn't launch upward
}

// ── Coulomb friction (tangential impulse) ───────────────────────────────────────

TEST(SimulationCore, FrictionDefaultsToZero) {
    RigidBodySolver s;
    EXPECT_FLOAT_EQ(s.friction(), 0.0f);
}

TEST(SimulationCore, SetFrictionRejectsNonFiniteAndClampsNegative) {
    RigidBodySolver s;
    s.setFriction(0.4f);
    EXPECT_FLOAT_EQ(s.friction(), 0.4f);
    s.setFriction(std::numeric_limits<float>::infinity()); // rejected, unchanged
    EXPECT_FLOAT_EQ(s.friction(), 0.4f);
    s.setFriction(-1.0f);                                  // clamped to 0
    EXPECT_FLOAT_EQ(s.friction(), 0.0f);
}

TEST(SimulationCore, FrictionlessSlidingBodyKeepsGlidingWithoutSpin) {
    // Frictionless: a sliding sphere keeps its tangential speed and never spins.
    RigidBodySolver s;
    s.setGravity({0.0f, -9.81f, 0.0f});
    s.setGroundPlane({0.0f, 1.0f, 0.0f}, 0.0f, 0.0f);
    s.setFriction(0.0f);
    const BodyId id = s.addBody({.mass = 1.0f, .position = {0.0f, 0.5f, 0.0f},
                                 .velocity = {2.0f, 0.0f, 0.0f}, .collisionRadius = 0.5f});
    for (int i = 0; i < 120; ++i) (void)s.step(0.01);
    SimVec3 p, v, w; SimQuat q;
    ASSERT_TRUE(s.getBodyState(id, p, v));
    ASSERT_TRUE(s.getBodyAngularState(id, q, w));
    EXPECT_NEAR(v.x, 2.0f, 1e-3f);   // tangential speed preserved
    EXPECT_NEAR(w.z, 0.0f, 1e-4f);   // no spin without friction
}

TEST(SimulationCore, GroundFrictionSpinsSlidingBodyUpToRolling) {
    // With friction the tangential impulse both slows the slide and spins the body
    // up until it rolls without slipping. For inertia {1,1,1}, m=1, r=0.5 the
    // rolling speed is v0 * m r^2 / (I + m r^2) = 2 * 0.25 / 1.25 = 0.4, and the
    // rolling condition is wz = -vx / r.
    RigidBodySolver s;
    s.setGravity({0.0f, -9.81f, 0.0f});
    s.setGroundPlane({0.0f, 1.0f, 0.0f}, 0.0f, 0.0f);
    s.setFriction(0.5f);
    const float r = 0.5f;
    const BodyId id = s.addBody({.mass = 1.0f, .position = {0.0f, 0.5f, 0.0f},
                                 .velocity = {2.0f, 0.0f, 0.0f},
                                 .inertia = {1.0f, 1.0f, 1.0f},
                                 .collisionRadius = r});
    for (int i = 0; i < 600; ++i) (void)s.step(0.01);
    SimVec3 p, v, w; SimQuat q;
    ASSERT_TRUE(s.getBodyState(id, p, v));
    ASSERT_TRUE(s.getBodyAngularState(id, q, w));
    EXPECT_NEAR(v.x, 0.4f, 0.05f);          // decelerated from 2, but rolling — not stopped
    EXPECT_GT(v.x, 0.0f);
    EXPECT_NEAR(w.z, -v.x / r, 0.05f);      // rolling without slipping (contact velocity ~ 0)
    EXPECT_LT(w.z, -0.1f);                  // genuinely spun up
}

TEST(SimulationCore, BodyBodyFrictionOpposesTangentialSlide) {
    // B presses into a static A along X (contact normal ~ +X) while sliding along Y.
    // The normal impulse leaves the tangential (Y) velocity alone; friction bleeds
    // it off. One small step keeps the contact normal axis-aligned for a clean read.
    auto tangentialVy = [](float friction) {
        RigidBodySolver s;
        s.setGravity({0.0f, 0.0f, 0.0f});
        s.setBodyCollision(0.0f);
        s.setFriction(friction);
        (void)s.addBody({.mass = 0.0f, .position = {0.0f, 0.0f, 0.0f}, .collisionRadius = 0.5f}); // static
        const BodyId b = s.addBody({.mass = 1.0f, .position = {0.8f, 0.0f, 0.0f},
                                    .velocity = {-1.0f, 3.0f, 0.0f}, .collisionRadius = 0.5f});
        (void)s.step(0.001);
        SimVec3 p, v;
        s.getBodyState(b, p, v);
        return v.y;
    };
    EXPECT_NEAR(tangentialVy(0.0f), 3.0f, 0.05f);   // frictionless: tangential speed preserved
    EXPECT_LT(tangentialVy(0.5f), 2.7f);            // friction removes tangential speed
    EXPECT_LT(tangentialVy(0.5f), tangentialVy(0.0f));
}

TEST(SimulationCore, BodyBodyFrictionImpartsSpin) {
    // B presses into a static A along X while sliding along +Y. The tangential
    // friction impulse acts at B's contact point, so it must also spin B (angular
    // coupling), not just slow its slide.
    RigidBodySolver s;
    s.setGravity({0.0f, 0.0f, 0.0f});
    s.setBodyCollision(0.0f);
    s.setFriction(0.5f);
    (void)s.addBody({.mass = 0.0f, .position = {0.0f, 0.0f, 0.0f}, .collisionRadius = 0.5f}); // static
    const BodyId b = s.addBody({.mass = 1.0f, .position = {0.8f, 0.0f, 0.0f},
                                .velocity = {-1.0f, 3.0f, 0.0f},
                                .inertia = {1.0f, 1.0f, 1.0f}, .collisionRadius = 0.5f});
    SimVec3 p, v, w; SimQuat q;
    ASSERT_TRUE(s.getBodyAngularState(b, q, w));
    EXPECT_NEAR(w.z, 0.0f, 1e-6f);          // starts unspun
    (void)s.step(0.001);
    ASSERT_TRUE(s.getBodyAngularState(b, q, w));
    EXPECT_GT(w.z, 0.0f);                    // friction at the contact spun it up about +Z
}
