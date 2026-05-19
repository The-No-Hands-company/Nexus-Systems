#include <gtest/gtest.h>
#include "nexus/sim/SimulationCore.h"

#include <cmath>
#include <limits>

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
