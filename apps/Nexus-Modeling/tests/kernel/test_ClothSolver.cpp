#include <gtest/gtest.h>
#include "nexus/sim/ClothSolver.h"

#include <limits>

using namespace nexus;

// ── ClothSolver node management ───────────────────────────────────────────────

TEST(ClothSolver, AddNodeReturnsUniqueIds) {
    ClothSolver solver;
    const auto a = solver.addNode({1.0f, {0,0,0}, {0,0,0}});
    const auto b = solver.addNode({1.0f, {1,0,0}, {0,0,0}});
    EXPECT_NE(a, kInvalidClothNodeId);
    EXPECT_NE(b, kInvalidClothNodeId);
    EXPECT_NE(a, b);
    EXPECT_EQ(solver.nodeCount(), 2u);
}

TEST(ClothSolver, RemoveNodeReducesCount) {
    ClothSolver solver;
    const auto id = solver.addNode({1.0f, {}, {}});
    ASSERT_TRUE(solver.hasNode(id));
    EXPECT_TRUE(solver.removeNode(id));
    EXPECT_FALSE(solver.hasNode(id));
    EXPECT_EQ(solver.nodeCount(), 0u);
}

TEST(ClothSolver, RemoveUnknownNodeReturnsFalse) {
    ClothSolver solver;
    EXPECT_FALSE(solver.removeNode(99u));
}

TEST(ClothSolver, PinnedNodeNotReturnedByGetState) {
    ClothSolver solver;
    const auto id = solver.addNode({0.0f, {1,2,3}, {0,0,0}});  // pinned
    ClothVec3 pos{}, vel{};
    EXPECT_FALSE(solver.getNodeState(id, pos, vel));
}

// ── ClothSolver gravity and step ──────────────────────────────────────────────

TEST(ClothSolver, GravityDefaultAndSetter) {
    ClothSolver solver;
    const ClothVec3 def = solver.gravity();
    EXPECT_FLOAT_EQ(def.y, -9.81f);

    solver.setGravity({0.0f, 0.0f, 0.0f});
    const ClothVec3 zero = solver.gravity();
    EXPECT_FLOAT_EQ(zero.y, 0.0f);
}

TEST(ClothSolver, StepWithNegativeDtFails) {
    ClothSolver solver;
    const auto id = solver.addNode({1.0f, {}, {}});
    EXPECT_NE(id, kInvalidClothNodeId);
    const auto r = solver.step(-0.016);
    EXPECT_FALSE(r.ok);
    EXPECT_EQ(r.nodesIntegrated, 0u);
}

TEST(ClothSolver, StepRejectsNonFiniteRuntimeState) {
    ClothSolver solver;
    const auto id = solver.addNode({1.0f, {}, {}});
    ASSERT_NE(id, kInvalidClothNodeId);

    solver.setGravity({std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f});

    const auto r = solver.step(0.016);
    EXPECT_FALSE(r.ok);
    EXPECT_EQ(r.nodesIntegrated, 0u);
}

TEST(ClothSolver, StepIntegratesDynamicNodes) {
    ClothSolver solver;
    solver.setGravity({0.0f, 0.0f, 0.0f});
    const auto id = solver.addNode({1.0f, {0,10,0}, {0,0,0}});
    const auto r  = solver.step(0.016);
    EXPECT_TRUE(r.ok);
    EXPECT_EQ(r.nodesIntegrated, 1u);
    EXPECT_GT(r.simulationTime, 0.0);
    ClothVec3 pos{}, vel{};
    EXPECT_TRUE(solver.getNodeState(id, pos, vel));
}

TEST(ClothSolver, PinnedNodeNotIntegrated) {
    ClothSolver solver;
    solver.setGravity({0.0f, -9.81f, 0.0f});
    const auto pinned  = solver.addNode({0.0f, {0,5,0}, {0,0,0}});
    const auto dynamic = solver.addNode({1.0f, {0,5,0}, {0,0,0}});
    const auto r = solver.step(0.016);
    EXPECT_EQ(r.nodesIntegrated, 1u);

    // Pinned node should not be readable via getNodeState.
    ClothVec3 p{}, v{};
    EXPECT_FALSE(solver.getNodeState(pinned, p, v));
    EXPECT_TRUE(solver.getNodeState(dynamic, p, v));
    // Dynamic node fell under gravity.
    EXPECT_LT(p.y, 5.0f);
}

// ── ClothSolver spring edge ───────────────────────────────────────────────────

TEST(ClothSolver, AddEdgeUnknownNodeReturnsFalse) {
    ClothSolver solver;
    const auto id = solver.addNode({1.0f, {0,0,0}, {}});
    EXPECT_FALSE(solver.addEdge(id, 99u, 1.0f, 100.0f));
}

TEST(ClothSolver, AddEdgeRejectsSelfEdgeAndInvalidParameters) {
    ClothSolver solver;
    const auto a = solver.addNode({1.0f, {0,0,0}, {}});
    const auto b = solver.addNode({1.0f, {1,0,0}, {}});
    ASSERT_NE(a, kInvalidClothNodeId);
    ASSERT_NE(b, kInvalidClothNodeId);

    EXPECT_FALSE(solver.addEdge(a, a, 1.0f, 100.0f));
    EXPECT_FALSE(solver.addEdge(a, b, -0.1f, 100.0f));
    EXPECT_FALSE(solver.addEdge(a, b, 1.0f, 0.0f));
    EXPECT_FALSE(solver.addEdge(a, b, 1.0f, -1.0f));
}

TEST(ClothSolver, AddEdgeRejectsDuplicateUndirectedEdge) {
    ClothSolver solver;
    const auto a = solver.addNode({1.0f, {0,0,0}, {}});
    const auto b = solver.addNode({1.0f, {1,0,0}, {}});
    ASSERT_TRUE(solver.addEdge(a, b, 1.0f, 100.0f));

    EXPECT_FALSE(solver.addEdge(a, b, 1.0f, 100.0f));
    EXPECT_FALSE(solver.addEdge(b, a, 1.0f, 100.0f));
}

TEST(ClothSolver, SpringEdgeAttracts) {
    // Two nodes placed 2m apart, restLength=1m, no gravity.
    // Spring should pull them closer.
    ClothSolver solver;
    solver.setGravity({0.0f, 0.0f, 0.0f});
    const auto a = solver.addNode({1.0f, {-1,0,0}, {0,0,0}});
    const auto b = solver.addNode({1.0f, { 1,0,0}, {0,0,0}});
    ASSERT_TRUE(solver.addEdge(a, b, 1.0f, 100.0f));

    EXPECT_TRUE(solver.step(0.016).ok);

    ClothVec3 pa{}, va{}, pb{}, vb{};
    ASSERT_TRUE(solver.getNodeState(a, pa, va));
    ASSERT_TRUE(solver.getNodeState(b, pb, vb));
    // a should have moved toward b (pos.x increases from -1).
    EXPECT_GT(pa.x, -1.0f);
    // b should have moved toward a (pos.x decreases from 1).
    EXPECT_LT(pb.x,  1.0f);
}

// ── ClothSolver snapshot/rollback ────────────────────────────────────────────

TEST(ClothSolver, CaptureAndRestorePreservesState) {
    ClothSolver solver;
    solver.setGravity({0.0f, -9.81f, 0.0f});
    const auto id = solver.addNode({1.0f, {0,10,0}, {0,0,0}});
    EXPECT_NE(id, kInvalidClothNodeId);
    EXPECT_TRUE(solver.step(0.016).ok);

    const ClothState snap = solver.captureState();
    EXPECT_TRUE(solver.step(0.016).ok);
    EXPECT_TRUE(solver.step(0.016).ok);

    ASSERT_TRUE(solver.restoreState(snap));
    const ClothState after = solver.captureState();
    EXPECT_TRUE(snap == after);
}

TEST(ClothSolver, DeterministicReplayProducesSameState) {
    auto runSolver = []() -> ClothState {
        ClothSolver solver;
        solver.setGravity({0.0f, -9.81f, 0.0f});
        const auto a = solver.addNode({1.0f, {0,5,0}, {0,0,0}});
        const auto b = solver.addNode({2.0f, {1,5,0}, {0,0,0}});
        if (a == kInvalidClothNodeId || b == kInvalidClothNodeId) {
            return {};
        }
        for (int i = 0; i < 10; ++i) {
            if (!solver.step(0.016).ok) {
                return {};
            }
        }
        return solver.captureState();
    };
    const ClothState r1 = runSolver();
    const ClothState r2 = runSolver();
    EXPECT_TRUE(r1 == r2);
}

// ── ClothState serialization ──────────────────────────────────────────────────

TEST(ClothSolver, SerializationRoundTripIsDeterministic) {
    ClothSolver solver;
    solver.setGravity({0.0f, -9.81f, 0.0f});
    // Insert in reverse id order to verify sorted serialization.
    const auto b = solver.addNode({2.0f, {1,5,0}, {0,0,0}});
    const auto a = solver.addNode({1.0f, {0,5,0}, {0,0,0}});
    EXPECT_GT(b, 0u);
    EXPECT_GT(a, 0u);
    EXPECT_TRUE(solver.step(0.032).ok);

    const ClothState snap    = solver.captureState();
    const auto       bytesA  = serializeClothState(snap);
    const auto       bytesB  = serializeClothState(snap);
    EXPECT_EQ(bytesA, bytesB);

    ClothState restored;
    ASSERT_TRUE(deserializeClothState(bytesA, restored));
    EXPECT_TRUE(snap == restored);

    const auto bytesC = serializeClothState(restored);
    EXPECT_EQ(bytesC, bytesA);
}

TEST(ClothSolver, DeserializeRejectsMalformedBlob) {
    ClothState out;
    // Empty.
    EXPECT_FALSE(deserializeClothState({}, out));
    // Partial header.
    EXPECT_FALSE(deserializeClothState({0x43, 0x53, 0x4c, 0x31, 0x01}, out));
}
