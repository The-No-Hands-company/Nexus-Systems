#include <gtest/gtest.h>
#include "nexus/eval/EvalGraph.h"

#include <algorithm>
#include <limits>

using namespace nexus;

// ── Node management ───────────────────────────────────────────────────────────

TEST(EvalGraph, EmptyGraphHasZeroNodes) {
    EvalGraph g;
    EXPECT_EQ(g.nodeCount(), 0u);
}

TEST(EvalGraph, AddNodeIncreasesCount) {
    EvalGraph g;
    (void)g.addNode(NodeKind::Geometry, "geo");
    EXPECT_EQ(g.nodeCount(), 1u);
}

TEST(EvalGraph, AddedNodeIsKnownWithCorrectKindAndName) {
    EvalGraph g;
    NodeId id = g.addNode(NodeKind::Animation, "anim");
    EXPECT_TRUE(g.hasNode(id));
    EXPECT_EQ(g.nodeKind(id), NodeKind::Animation);
    EXPECT_EQ(g.nodeName(id), "anim");
}

TEST(EvalGraph, UnknownNodeReturnsFalseForHasNode) {
    EvalGraph g;
    EXPECT_FALSE(g.hasNode(42u));
}

TEST(EvalGraph, NodeKindReturnsConstantForUnknownId) {
    EvalGraph g;
    EXPECT_EQ(g.nodeKind(99u), NodeKind::Constant);
}

TEST(EvalGraph, NodeNameReturnsEmptyForUnknownId) {
    EvalGraph g;
    EXPECT_EQ(g.nodeName(99u), "");
}

TEST(EvalGraph, RemoveNodeDecreasesCount) {
    EvalGraph g;
    NodeId id = g.addNode(NodeKind::Transform, "t");
    EXPECT_TRUE(g.removeNode(id));
    EXPECT_EQ(g.nodeCount(), 0u);
    EXPECT_FALSE(g.hasNode(id));
}

TEST(EvalGraph, RemoveUnknownNodeReturnsFalse) {
    EvalGraph g;
    EXPECT_FALSE(g.removeNode(99u));
}

TEST(EvalGraph, NodeIdsAreStableAndUnique) {
    EvalGraph g;
    NodeId a = g.addNode(NodeKind::Geometry, "a");
    NodeId b = g.addNode(NodeKind::Geometry, "b");
    EXPECT_NE(a, b);
    EXPECT_NE(a, kInvalidNodeId);
    EXPECT_NE(b, kInvalidNodeId);
}

TEST(EvalGraph, ClearRemovesAllNodes) {
    EvalGraph g;
    (void)g.addNode(NodeKind::Merge, "m1");
    (void)g.addNode(NodeKind::Merge, "m2");
    g.clear();
    EXPECT_EQ(g.nodeCount(), 0u);
}

// ── Edge management ───────────────────────────────────────────────────────────

TEST(EvalGraph, ConnectTwoKnownNodes) {
    EvalGraph g;
    NodeId a = g.addNode(NodeKind::Constant, "c");
    NodeId b = g.addNode(NodeKind::Geometry, "g");
    EXPECT_TRUE(g.connect(a, b));
    EXPECT_TRUE(g.isConnected(a, b));
}

TEST(EvalGraph, ConnectUnknownNodeReturnsFalse) {
    EvalGraph g;
    NodeId a = g.addNode(NodeKind::Constant, "c");
    EXPECT_FALSE(g.connect(a, 99u));
    EXPECT_FALSE(g.connect(99u, a));
}

TEST(EvalGraph, ConnectSelfReturnsFalse) {
    EvalGraph g;
    NodeId a = g.addNode(NodeKind::Geometry, "g");
    EXPECT_FALSE(g.connect(a, a));
}

TEST(EvalGraph, ConnectDuplicateEdgeReturnsFalse) {
    EvalGraph g;
    NodeId a = g.addNode(NodeKind::Constant, "c");
    NodeId b = g.addNode(NodeKind::Geometry, "g");
    ASSERT_TRUE(g.connect(a, b));
    EXPECT_FALSE(g.connect(a, b));
}

TEST(EvalGraph, DisconnectRemovesEdge) {
    EvalGraph g;
    NodeId a = g.addNode(NodeKind::Constant, "c");
    NodeId b = g.addNode(NodeKind::Geometry, "g");
    ASSERT_TRUE(g.connect(a, b));
    EXPECT_TRUE(g.disconnect(a, b));
    EXPECT_FALSE(g.isConnected(a, b));
}

TEST(EvalGraph, DisconnectAbsentEdgeReturnsFalse) {
    EvalGraph g;
    NodeId a = g.addNode(NodeKind::Constant, "c");
    NodeId b = g.addNode(NodeKind::Geometry, "g");
    EXPECT_FALSE(g.disconnect(a, b));
}

TEST(EvalGraph, RemoveNodeAlsoRemovesItsEdges) {
    EvalGraph g;
    NodeId a = g.addNode(NodeKind::Constant, "c");
    NodeId b = g.addNode(NodeKind::Geometry, "g");
    NodeId c = g.addNode(NodeKind::Transform, "t");
    ASSERT_TRUE(g.connect(a, b));
    ASSERT_TRUE(g.connect(b, c));
    (void)g.removeNode(b);
    EXPECT_FALSE(g.isConnected(a, b));
    EXPECT_FALSE(g.isConnected(b, c));
}

TEST(EvalGraph, EdgeDirectionIsNotSymmetric) {
    EvalGraph g;
    NodeId a = g.addNode(NodeKind::Constant, "c");
    NodeId b = g.addNode(NodeKind::Geometry, "g");
    ASSERT_TRUE(g.connect(a, b));
    EXPECT_TRUE(g.isConnected(a, b));
    EXPECT_FALSE(g.isConnected(b, a));
}

// ── Cache invalidation ────────────────────────────────────────────────────────

TEST(EvalGraph, NewNodeStartsDirty) {
    EvalGraph g;
    NodeId id = g.addNode(NodeKind::Geometry, "g");
    EXPECT_TRUE(g.isDirty(id));
}

TEST(EvalGraph, ClearDirtyAllMakesAllNodesClean) {
    EvalGraph g;
    NodeId a = g.addNode(NodeKind::Geometry, "a");
    NodeId b = g.addNode(NodeKind::Animation, "b");
    g.clearDirtyAll();
    EXPECT_FALSE(g.isDirty(a));
    EXPECT_FALSE(g.isDirty(b));
}

TEST(EvalGraph, MarkDirtyDirtiesNodeAndTransitiveDownstream) {
    EvalGraph g;
    NodeId a = g.addNode(NodeKind::Constant, "c");
    NodeId b = g.addNode(NodeKind::Geometry, "g");
    NodeId c = g.addNode(NodeKind::Transform, "t");
    ASSERT_TRUE(g.connect(a, b));
    ASSERT_TRUE(g.connect(b, c));
    g.clearDirtyAll();

    g.markDirty(a);
    EXPECT_TRUE(g.isDirty(a));
    EXPECT_TRUE(g.isDirty(b));
    EXPECT_TRUE(g.isDirty(c));
}

TEST(EvalGraph, MarkDirtyMidChainDoesNotDirtyUpstream) {
    EvalGraph g;
    NodeId a = g.addNode(NodeKind::Constant, "c");
    NodeId b = g.addNode(NodeKind::Geometry, "g");
    NodeId c = g.addNode(NodeKind::Transform, "t");
    ASSERT_TRUE(g.connect(a, b));
    ASSERT_TRUE(g.connect(b, c));
    g.clearDirtyAll();

    g.markDirty(b);
    EXPECT_FALSE(g.isDirty(a)); // upstream — must not be dirtied
    EXPECT_TRUE(g.isDirty(b));
    EXPECT_TRUE(g.isDirty(c));
}

TEST(EvalGraph, IsDirtyReturnsFalseForUnknownNode) {
    EvalGraph g;
    EXPECT_FALSE(g.isDirty(99u));
}

// ── Cycle detection ───────────────────────────────────────────────────────────

TEST(EvalGraph, EmptyGraphHasNoCycle) {
    EvalGraph g;
    EXPECT_FALSE(g.hasCycle());
}

TEST(EvalGraph, AcyclicGraphHasNoCycle) {
    EvalGraph g;
    NodeId a = g.addNode(NodeKind::Constant, "c");
    NodeId b = g.addNode(NodeKind::Geometry, "g");
    ASSERT_TRUE(g.connect(a, b));
    EXPECT_FALSE(g.hasCycle());
}

TEST(EvalGraph, DirectCycleIsDetected) {
    EvalGraph g;
    NodeId a = g.addNode(NodeKind::Geometry, "a");
    NodeId b = g.addNode(NodeKind::Geometry, "b");
    ASSERT_TRUE(g.connect(a, b));
    ASSERT_TRUE(g.connect(b, a));
    EXPECT_TRUE(g.hasCycle());
}

TEST(EvalGraph, LongerCycleIsDetected) {
    EvalGraph g;
    NodeId a = g.addNode(NodeKind::Geometry, "a");
    NodeId b = g.addNode(NodeKind::Transform, "b");
    NodeId c = g.addNode(NodeKind::Merge, "c");
    ASSERT_TRUE(g.connect(a, b));
    ASSERT_TRUE(g.connect(b, c));
    ASSERT_TRUE(g.connect(c, a));
    EXPECT_TRUE(g.hasCycle());
}

TEST(EvalGraph, DisconnectBreaksCycleAndHasCycleReturnsFalse) {
    EvalGraph g;
    NodeId a = g.addNode(NodeKind::Geometry, "a");
    NodeId b = g.addNode(NodeKind::Geometry, "b");
    ASSERT_TRUE(g.connect(a, b));
    ASSERT_TRUE(g.connect(b, a));
    ASSERT_TRUE(g.hasCycle());
    (void)g.disconnect(b, a);
    EXPECT_FALSE(g.hasCycle());
}

// ── Evaluation ────────────────────────────────────────────────────────────────

TEST(EvalGraph, EvaluateEmptyGraphSucceeds) {
    EvalGraph g;
    auto r = g.evaluate();
    EXPECT_TRUE(r.ok);
    EXPECT_FALSE(r.hasCycle);
    EXPECT_TRUE(r.evaluationOrder.empty());
    EXPECT_TRUE(r.dirtyNodes.empty());
}

TEST(EvalGraph, EvaluateProducesDependencyFirstOrder) {
    EvalGraph g;
    NodeId a = g.addNode(NodeKind::Constant, "c");
    NodeId b = g.addNode(NodeKind::Geometry, "g");
    NodeId c = g.addNode(NodeKind::Transform, "t");
    ASSERT_TRUE(g.connect(a, b));
    ASSERT_TRUE(g.connect(b, c));

    auto r = g.evaluate();
    EXPECT_TRUE(r.ok);
    ASSERT_EQ(r.evaluationOrder.size(), 3u);

    auto posA = std::find(r.evaluationOrder.begin(), r.evaluationOrder.end(), a);
    auto posB = std::find(r.evaluationOrder.begin(), r.evaluationOrder.end(), b);
    auto posC = std::find(r.evaluationOrder.begin(), r.evaluationOrder.end(), c);
    EXPECT_LT(posA, posB);
    EXPECT_LT(posB, posC);
}

TEST(EvalGraph, EvaluateMarksAllNewNodesDirtyOnFirstPass) {
    EvalGraph g;
    NodeId a = g.addNode(NodeKind::Constant, "c");
    NodeId b = g.addNode(NodeKind::Geometry, "g");
    ASSERT_TRUE(g.connect(a, b));

    auto r = g.evaluate();
    EXPECT_EQ(r.dirtyNodes.size(), 2u);
}

TEST(EvalGraph, EvaluateSecondPassHasNoDirtyNodes) {
    EvalGraph g;
    NodeId a = g.addNode(NodeKind::Constant, "c");
    NodeId b = g.addNode(NodeKind::Geometry, "g");
    ASSERT_TRUE(g.connect(a, b));

    (void)g.evaluate(); // first pass clears dirty
    auto r2 = g.evaluate();
    EXPECT_TRUE(r2.ok);
    EXPECT_TRUE(r2.dirtyNodes.empty());
}

TEST(EvalGraph, EvaluateAfterMarkDirtyReEvaluatesDownstream) {
    EvalGraph g;
    NodeId a = g.addNode(NodeKind::Constant, "c");
    NodeId b = g.addNode(NodeKind::Geometry, "g");
    NodeId c = g.addNode(NodeKind::Transform, "t");
    ASSERT_TRUE(g.connect(a, b));
    ASSERT_TRUE(g.connect(b, c));
    (void)g.evaluate();   // first pass — clears all dirty

    g.markDirty(a); // cascades to b and c
    auto r = g.evaluate();
    EXPECT_EQ(r.dirtyNodes.size(), 3u);
}

TEST(EvalGraph, EvaluatePartialDirtyOnlyReEvaluatesDirtyBranch) {
    EvalGraph g;
    NodeId a    = g.addNode(NodeKind::Constant, "c");
    NodeId bClean = g.addNode(NodeKind::Geometry, "clean");
    NodeId cDirty = g.addNode(NodeKind::Animation, "dirty");
    ASSERT_TRUE(g.connect(a, bClean));
    ASSERT_TRUE(g.connect(a, cDirty));
    (void)g.evaluate(); // clears all

    g.markDirty(cDirty); // only cDirty and its downstream (none) are dirty
    auto r = g.evaluate();
    ASSERT_EQ(r.dirtyNodes.size(), 1u);
    EXPECT_EQ(r.dirtyNodes[0], cDirty);
}

TEST(EvalGraph, EvaluateWithCycleFailsAndReportsHasCycle) {
    EvalGraph g;
    NodeId a = g.addNode(NodeKind::Geometry, "a");
    NodeId b = g.addNode(NodeKind::Geometry, "b");
    ASSERT_TRUE(g.connect(a, b));
    ASSERT_TRUE(g.connect(b, a));

    auto r = g.evaluate();
    EXPECT_FALSE(r.ok);
    EXPECT_TRUE(r.hasCycle);
    EXPECT_TRUE(r.evaluationOrder.empty());
}

TEST(EvalGraph, EvaluationOrderIsDeterministicAcrossRepeatedRuns) {
    auto build = []() {
        EvalGraph g;
        NodeId a = g.addNode(NodeKind::Constant, "c");
        NodeId b = g.addNode(NodeKind::Geometry, "g1");
        NodeId c = g.addNode(NodeKind::Geometry, "g2");
        NodeId d = g.addNode(NodeKind::Merge, "m");
        (void)g.connect(a, b);
        (void)g.connect(a, c);
        (void)g.connect(b, d);
        (void)g.connect(c, d);
        return g.evaluate().evaluationOrder;
    };
    EXPECT_EQ(build(), build());
}

TEST(EvalGraph, DiamondDependencyEvaluatesRootBeforeLeaf) {
    EvalGraph g;
    NodeId root  = g.addNode(NodeKind::Constant, "root");
    NodeId left  = g.addNode(NodeKind::Geometry, "left");
    NodeId right = g.addNode(NodeKind::Geometry, "right");
    NodeId merge = g.addNode(NodeKind::Merge, "merge");
    ASSERT_TRUE(g.connect(root, left));
    ASSERT_TRUE(g.connect(root, right));
    ASSERT_TRUE(g.connect(left, merge));
    ASSERT_TRUE(g.connect(right, merge));

    auto r = g.evaluate();
    EXPECT_TRUE(r.ok);
    auto posRoot  = std::find(r.evaluationOrder.begin(), r.evaluationOrder.end(), root);
    auto posMerge = std::find(r.evaluationOrder.begin(), r.evaluationOrder.end(), merge);
    EXPECT_LT(posRoot, posMerge);
}

TEST(EvalGraph, ComputeCallbackInvokedInEvaluationOrderForDirtyNodes) {
    EvalGraph g;
    NodeId a = g.addNode(NodeKind::Constant, "a");
    NodeId b = g.addNode(NodeKind::Geometry, "b");
    NodeId c = g.addNode(NodeKind::Transform, "c");
    ASSERT_TRUE(g.connect(a, b));
    ASSERT_TRUE(g.connect(b, c));

    std::vector<NodeId> invoked;
    g.setComputeCallback([&](NodeId id, NodeKind, const std::string&) {
        invoked.push_back(id);
        return true;
    });

    const auto r = g.evaluate();
    EXPECT_TRUE(r.ok);
    EXPECT_EQ(invoked, r.evaluationOrder);
    EXPECT_EQ(invoked, r.dirtyNodes);
}

TEST(EvalGraph, ComputeCallbackRunsOnlyForDirtyNodes) {
    EvalGraph g;
    NodeId a = g.addNode(NodeKind::Constant, "a");
    NodeId b = g.addNode(NodeKind::Geometry, "b");
    NodeId c = g.addNode(NodeKind::Transform, "c");
    ASSERT_TRUE(g.connect(a, b));
    ASSERT_TRUE(g.connect(b, c));

    (void)g.evaluate(); // clear initial dirty state
    g.markDirty(b);

    std::vector<NodeId> invoked;
    g.setComputeCallback([&](NodeId id, NodeKind, const std::string&) {
        invoked.push_back(id);
        return true;
    });

    const auto r = g.evaluate();
    EXPECT_TRUE(r.ok);
    EXPECT_EQ(invoked.size(), 2u);
    EXPECT_EQ(invoked[0], b);
    EXPECT_EQ(invoked[1], c);
    EXPECT_EQ(r.dirtyNodes, invoked);
}

TEST(EvalGraph, ComputeCallbackFailureAbortsEvaluationAndReportsNode) {
    EvalGraph g;
    NodeId a = g.addNode(NodeKind::Constant, "a");
    NodeId b = g.addNode(NodeKind::Geometry, "b");
    NodeId c = g.addNode(NodeKind::Transform, "c");
    ASSERT_TRUE(g.connect(a, b));
    ASSERT_TRUE(g.connect(b, c));

    std::vector<NodeId> invoked;
    g.setComputeCallback([&](NodeId id, NodeKind, const std::string&) {
        invoked.push_back(id);
        return id != b;
    });

    const auto r = g.evaluate();
    EXPECT_FALSE(r.ok);
    EXPECT_FALSE(r.hasCycle);
    EXPECT_TRUE(r.executionFailed);
    EXPECT_EQ(r.failedNode, b);
    ASSERT_EQ(invoked.size(), 2u);
    EXPECT_EQ(invoked[0], a);
    EXPECT_EQ(invoked[1], b);

    // Nodes evaluated before failure are clean; failed and downstream remain dirty.
    EXPECT_FALSE(g.isDirty(a));
    EXPECT_TRUE(g.isDirty(b));
    EXPECT_TRUE(g.isDirty(c));
}

TEST(EvalGraph, ComputeCallbackReceivesDeterministicDependencyContext) {
    EvalGraph g;
    NodeId a = g.addNode(NodeKind::Constant, "a");
    NodeId b = g.addNode(NodeKind::Geometry, "b");
    NodeId c = g.addNode(NodeKind::Geometry, "c");
    NodeId d = g.addNode(NodeKind::Merge, "d");
    ASSERT_TRUE(g.connect(a, b));
    ASSERT_TRUE(g.connect(a, c));
    ASSERT_TRUE(g.connect(b, d));
    ASSERT_TRUE(g.connect(c, d));

    std::vector<NodeComputeContext> contexts;
    g.setComputeCallback([&](const NodeComputeContext& context) {
        contexts.push_back(context);
        return true;
    });

    const auto r = g.evaluate();
    EXPECT_TRUE(r.ok);
    ASSERT_EQ(contexts.size(), r.evaluationOrder.size());

    for (std::size_t i = 0; i < contexts.size(); ++i) {
        EXPECT_EQ(contexts[i].id, r.evaluationOrder[i]);
        EXPECT_EQ(contexts[i].evaluationIndex, i);
    }

    auto it = std::find_if(contexts.begin(), contexts.end(), [d](const NodeComputeContext& ctx) {
        return ctx.id == d;
    });
    ASSERT_NE(it, contexts.end());
    ASSERT_EQ(it->inputNodes.size(), 2u);
    EXPECT_EQ(it->inputNodes[0], b);
    EXPECT_EQ(it->inputNodes[1], c);
    EXPECT_TRUE(it->outputNodes.empty());
}

TEST(EvalGraph, CycleFailureHasDeterministicMessage) {
    EvalGraph g;
    NodeId a = g.addNode(NodeKind::Geometry, "a");
    NodeId b = g.addNode(NodeKind::Geometry, "b");
    ASSERT_TRUE(g.connect(a, b));
    ASSERT_TRUE(g.connect(b, a));

    const auto r = g.evaluate();
    EXPECT_FALSE(r.ok);
    EXPECT_TRUE(r.hasCycle);
    ASSERT_EQ(r.messages.size(), 1u);
    EXPECT_EQ(r.messages[0], "cycle detected: evaluation aborted before compute dispatch");
}

TEST(EvalGraph, ComputeFailureMessageContainsDeterministicContext) {
    EvalGraph g;
    NodeId a = g.addNode(NodeKind::Constant, "root");
    NodeId b = g.addNode(NodeKind::Geometry, "mid");
    NodeId c = g.addNode(NodeKind::Transform, "leaf");
    ASSERT_TRUE(g.connect(a, b));
    ASSERT_TRUE(g.connect(b, c));

    g.setComputeCallback([&](const NodeComputeContext& context) {
        return context.id != b;
    });

    const auto r = g.evaluate();
    EXPECT_FALSE(r.ok);
    EXPECT_TRUE(r.executionFailed);
    EXPECT_EQ(r.failedNode, b);
    ASSERT_EQ(r.messages.size(), 1u);
    EXPECT_NE(r.messages[0].find("node=" + std::to_string(b)), std::string::npos);
    EXPECT_NE(r.messages[0].find("kind=Geometry"), std::string::npos);
    EXPECT_NE(r.messages[0].find("name=mid"), std::string::npos);
    EXPECT_NE(r.messages[0].find("inputs=[" + std::to_string(a) + "]"), std::string::npos);
    EXPECT_NE(r.messages[0].find("outputs=[" + std::to_string(c) + "]"), std::string::npos);
}

TEST(EvalGraph, ComputeFailureMessageUsesProxyGeometryKindName) {
    EvalGraph g;
    NodeId n = g.addNode(NodeKind::ProxyGeometry, "proxy");

    g.setComputeCallback([&](const NodeComputeContext& context) {
        return context.id != n;
    });

    const auto r = g.evaluate();
    EXPECT_FALSE(r.ok);
    EXPECT_TRUE(r.executionFailed);
    EXPECT_EQ(r.failedNode, n);
    ASSERT_EQ(r.messages.size(), 1u);
    EXPECT_NE(r.messages[0].find("kind=ProxyGeometry"), std::string::npos);
}

TEST(EvalGraph, ComputeFailureMessageUsesReconstructionKindName) {
    EvalGraph g;
    NodeId n = g.addNode(NodeKind::Reconstruction, "recon");

    g.setComputeCallback([&](const NodeComputeContext& context) {
        return context.id != n;
    });

    const auto r = g.evaluate();
    EXPECT_FALSE(r.ok);
    EXPECT_TRUE(r.executionFailed);
    EXPECT_EQ(r.failedNode, n);
    ASSERT_EQ(r.messages.size(), 1u);
    EXPECT_NE(r.messages[0].find("kind=Reconstruction"), std::string::npos);
}

TEST(EvalGraph, CallbackCanEmitDeterministicDiagnosticsMessages) {
    EvalGraph g;
    NodeId src = g.addNode(NodeKind::Constant, "src");
    NodeId recon = g.addNode(NodeKind::Reconstruction, "fit");
    ASSERT_TRUE(g.connect(src, recon));

    g.setComputeCallback([&](NodeComputeContext& context) -> bool {
        if (context.id == recon) {
            if (context.diagnostics == nullptr) {
                return false;
            }
            context.diagnostics->push_back(
                "reconstruction residual=0.125 confidence=0.875 node=" + std::to_string(context.id));
        }
        return true;
    });

    const auto r = g.evaluate();
    EXPECT_TRUE(r.ok);
    ASSERT_EQ(r.messages.size(), 1u);
    EXPECT_EQ(r.messages[0], "reconstruction residual=0.125 confidence=0.875 node=" + std::to_string(recon));
}

TEST(EvalGraph, CallbackDiagnosticsOrderMatchesEvaluationOrder) {
    EvalGraph g;
    NodeId a = g.addNode(NodeKind::Constant, "a");
    NodeId b = g.addNode(NodeKind::Reconstruction, "b");
    NodeId c = g.addNode(NodeKind::ProxyGeometry, "c");
    ASSERT_TRUE(g.connect(a, b));
    ASSERT_TRUE(g.connect(b, c));

    g.setComputeCallback([&](NodeComputeContext& context) -> bool {
        if (context.diagnostics == nullptr) {
            return false;
        }
        context.diagnostics->push_back("diag node=" + std::to_string(context.id));
        return true;
    });

    const auto r = g.evaluate();
    EXPECT_TRUE(r.ok);
    ASSERT_EQ(r.evaluationOrder.size(), 3u);
    ASSERT_EQ(r.messages.size(), 3u);
    EXPECT_EQ(r.messages[0], "diag node=" + std::to_string(r.evaluationOrder[0]));
    EXPECT_EQ(r.messages[1], "diag node=" + std::to_string(r.evaluationOrder[1]));
    EXPECT_EQ(r.messages[2], "diag node=" + std::to_string(r.evaluationOrder[2]));
}

TEST(EvalGraph, LegacyCallbackRegistrationRemainsSupported) {
    EvalGraph g;
    NodeId a = g.addNode(NodeKind::Constant, "a");
    NodeId b = g.addNode(NodeKind::Geometry, "b");
    ASSERT_TRUE(g.connect(a, b));

    std::vector<NodeId> invoked;
    g.setComputeCallback([&](NodeId id, NodeKind, const std::string&) {
        invoked.push_back(id);
        return true;
    });

    const auto r = g.evaluate();
    EXPECT_TRUE(r.ok);
    ASSERT_EQ(invoked.size(), 2u);
    EXPECT_EQ(invoked[0], a);
    EXPECT_EQ(invoked[1], b);
}

TEST(EvalGraph, NodePayloadSlotSetAndGetTypedValues) {
    EvalGraph g;
    NodeId n = g.addNode(NodeKind::Constant, "payload");

    NodePayload p;
    p.value = 3.5f;
    ASSERT_TRUE(g.setNodeOutputPayload(n, p));

    const NodePayload* out = g.nodeOutputPayload(n);
    ASSERT_NE(out, nullptr);
    ASSERT_EQ(out->type(), NodePayloadType::ScalarF32);
    ASSERT_NE(out->scalarF32(), nullptr);
    EXPECT_FLOAT_EQ(*out->scalarF32(), 3.5f);

    p.value = int64_t{42};
    ASSERT_TRUE(g.setNodeOutputPayload(n, p));
    out = g.nodeOutputPayload(n);
    ASSERT_NE(out, nullptr);
    ASSERT_EQ(out->type(), NodePayloadType::IntegerI64);
    ASSERT_NE(out->integerI64(), nullptr);
    EXPECT_EQ(*out->integerI64(), 42);
}

TEST(EvalGraph, CallbackReadsUpstreamPayloadAndWritesOutputPayload) {
    EvalGraph g;
    NodeId src = g.addNode(NodeKind::Constant, "src");
    NodeId dst = g.addNode(NodeKind::Transform, "dst");
    ASSERT_TRUE(g.connect(src, dst));

    NodePayload seed;
    seed.value = 10.0f;
    ASSERT_TRUE(g.setNodeOutputPayload(src, seed));

    g.setComputeCallback([&](NodeComputeContext& context) -> bool {
        if (context.id == dst) {
            if (context.inputPayloads.size() != 1u) return false;
            if (context.inputPayloads[0].inputNode != src) return false;
            if (context.inputPayloads[0].payload == nullptr) return false;
            const float* in = context.inputPayloads[0].payload->scalarF32();
            if (in == nullptr) return false;
            if (context.outputPayload == nullptr) return false;
            context.outputPayload->value = (*in) * 2.0f;
        }
        return true;
    });

    const auto r = g.evaluate();
    EXPECT_TRUE(r.ok);

    const NodePayload* out = g.nodeOutputPayload(dst);
    ASSERT_NE(out, nullptr);
    ASSERT_EQ(out->type(), NodePayloadType::ScalarF32);
    ASSERT_NE(out->scalarF32(), nullptr);
    EXPECT_FLOAT_EQ(*out->scalarF32(), 20.0f);
}

TEST(EvalGraph, ClearAndRemoveNodeClearPayloadSlots) {
    EvalGraph g;
    NodeId a = g.addNode(NodeKind::Constant, "a");
    NodeId b = g.addNode(NodeKind::Geometry, "b");

    NodePayload p;
    p.value = std::string("hello");
    ASSERT_TRUE(g.setNodeOutputPayload(a, p));
    ASSERT_TRUE(g.setNodeOutputPayload(b, p));

    ASSERT_TRUE(g.removeNode(a));
    EXPECT_EQ(g.nodeOutputPayload(a), nullptr);

    g.clear();
    EXPECT_EQ(g.nodeOutputPayload(b), nullptr);
}

TEST(EvalGraph, MultiInputPayloadOrderingStableAcrossEdgeInsertionOrder) {
    auto evaluateInputOrder = [](bool reverseEdgeInsertOrder) {
        EvalGraph g;
        NodeId a = g.addNode(NodeKind::Constant, "a");
        NodeId b = g.addNode(NodeKind::Constant, "b");
        NodeId m = g.addNode(NodeKind::Merge, "m");

        if (reverseEdgeInsertOrder) {
            EXPECT_TRUE(g.connect(b, m));
            EXPECT_TRUE(g.connect(a, m));
        } else {
            EXPECT_TRUE(g.connect(a, m));
            EXPECT_TRUE(g.connect(b, m));
        }

        NodePayload pa;
        pa.value = 1.0f;
        NodePayload pb;
        pb.value = 2.0f;
        EXPECT_TRUE(g.setNodeOutputPayload(a, pa));
        EXPECT_TRUE(g.setNodeOutputPayload(b, pb));

        std::vector<NodeId> observedInputNodes;
        std::vector<float> observedInputValues;
        g.setComputeCallback([&](NodeComputeContext& context) -> bool {
            if (context.id != m) {
                return true;
            }

            observedInputNodes.clear();
            observedInputValues.clear();
            for (const NodeInputPayload& entry : context.inputPayloads) {
                if (entry.payload == nullptr) return false;
                const float* v = entry.payload->scalarF32();
                if (v == nullptr) return false;
                observedInputNodes.push_back(entry.inputNode);
                observedInputValues.push_back(*v);
            }

            if (context.outputPayload == nullptr) return false;
            context.outputPayload->value = 3.0f;
            return true;
        });

        const EvalReport r = g.evaluate();
        EXPECT_TRUE(r.ok);
        return std::make_pair(observedInputNodes, observedInputValues);
    };

    const auto forward = evaluateInputOrder(false);
    const auto reverse = evaluateInputOrder(true);

    ASSERT_EQ(forward.first.size(), 2u);
    ASSERT_EQ(reverse.first.size(), 2u);
    EXPECT_EQ(forward.first, reverse.first);
    EXPECT_EQ(forward.second, reverse.second);

    EXPECT_LT(forward.first[0], forward.first[1]);
    EXPECT_FLOAT_EQ(forward.second[0], 1.0f);
    EXPECT_FLOAT_EQ(forward.second[1], 2.0f);
}

TEST(EvalGraph, UnknownNodePayloadApisReturnDeterministicFailureValues) {
    EvalGraph g;
    NodeId known = g.addNode(NodeKind::Constant, "known");
    const NodeId unknown = known + 999u;

    NodePayload p;
    p.value = 9.0f;

    EXPECT_FALSE(g.setNodeOutputPayload(unknown, p));
    EXPECT_FALSE(g.clearNodeOutputPayload(unknown));
    EXPECT_EQ(g.nodeOutputPayload(unknown), nullptr);

    const EvalGraph& cg = g;
    EXPECT_EQ(cg.nodeOutputPayload(unknown), nullptr);
}

TEST(EvalGraph, ClearNodeOutputPayloadResetsTypeToNone) {
    EvalGraph g;
    NodeId n = g.addNode(NodeKind::Constant, "n");

    NodePayload p;
    p.value = std::string("value");
    ASSERT_TRUE(g.setNodeOutputPayload(n, p));

    const NodePayload* beforeClear = g.nodeOutputPayload(n);
    ASSERT_NE(beforeClear, nullptr);
    EXPECT_EQ(beforeClear->type(), NodePayloadType::TextUtf8);

    ASSERT_TRUE(g.clearNodeOutputPayload(n));

    const NodePayload* afterClear = g.nodeOutputPayload(n);
    ASSERT_NE(afterClear, nullptr);
    EXPECT_EQ(afterClear->type(), NodePayloadType::None);
    EXPECT_EQ(afterClear->scalarF32(), nullptr);
    EXPECT_EQ(afterClear->integerI64(), nullptr);
    EXPECT_EQ(afterClear->boolean(), nullptr);
    EXPECT_EQ(afterClear->textUtf8(), nullptr);
    EXPECT_EQ(afterClear->binary(), nullptr);
    EXPECT_EQ(afterClear->splatCloud(), nullptr);
    EXPECT_EQ(afterClear->reconstructionDiagnostic(), nullptr);
}

TEST(EvalGraph, BinaryPayloadRoundTripThroughNodeSlot) {
    EvalGraph g;
    NodeId n = g.addNode(NodeKind::Geometry, "blob");

    NodePayload p;
    p.value = NodePayload::Binary{0x01, 0x02, 0x03, 0xFF};
    ASSERT_TRUE(g.setNodeOutputPayload(n, p));

    const NodePayload* out = g.nodeOutputPayload(n);
    ASSERT_NE(out, nullptr);
    ASSERT_EQ(out->type(), NodePayloadType::Binary);
    const NodePayload::Binary* b = out->binary();
    ASSERT_NE(b, nullptr);
    ASSERT_EQ(b->size(), 4u);
    EXPECT_EQ((*b)[0], 0x01);
    EXPECT_EQ((*b)[1], 0x02);
    EXPECT_EQ((*b)[2], 0x03);
    EXPECT_EQ((*b)[3], 0xFF);

    // Mutable accessor returns the same bytes.
    NodePayload::Binary* bMut = g.nodeOutputPayload(n)->binary();
    ASSERT_NE(bMut, nullptr);
    (*bMut)[0] = 0xAB;
    EXPECT_EQ(*g.nodeOutputPayload(n)->binary(), (NodePayload::Binary{0xAB, 0x02, 0x03, 0xFF}));
}

TEST(EvalGraph, SplatCloudPayloadRoundTripThroughNodeSlot) {
    EvalGraph g;
    NodeId n = g.addNode(NodeKind::Reconstruction, "cloud");

    NodePayload p;
    p.value = NodePayload::SplatCloud{
        NodePayload::Splat{0.0f, 1.0f, 2.0f, 0.25f, 0.90f},
        NodePayload::Splat{3.0f, 4.0f, 5.0f, 0.50f, 0.75f}
    };
    ASSERT_TRUE(g.setNodeOutputPayload(n, p));

    const NodePayload* out = g.nodeOutputPayload(n);
    ASSERT_NE(out, nullptr);
    ASSERT_EQ(out->type(), NodePayloadType::SplatCloud);

    const NodePayload::SplatCloud* cloud = out->splatCloud();
    ASSERT_NE(cloud, nullptr);
    ASSERT_EQ(cloud->size(), 2u);
    EXPECT_FLOAT_EQ((*cloud)[0].x, 0.0f);
    EXPECT_FLOAT_EQ((*cloud)[0].y, 1.0f);
    EXPECT_FLOAT_EQ((*cloud)[0].z, 2.0f);
    EXPECT_FLOAT_EQ((*cloud)[0].radius, 0.25f);
    EXPECT_FLOAT_EQ((*cloud)[0].opacity, 0.90f);

    NodePayload::SplatCloud* cloudMut = g.nodeOutputPayload(n)->splatCloud();
    ASSERT_NE(cloudMut, nullptr);
    (*cloudMut)[1].opacity = 0.50f;
    EXPECT_FLOAT_EQ(g.nodeOutputPayload(n)->splatCloud()->at(1).opacity, 0.50f);
}

TEST(EvalGraph, ReconstructionDiagnosticRoundTripThroughNodeSlot) {
    EvalGraph g;
    NodeId n = g.addNode(NodeKind::Reconstruction, "diag");

    NodePayload p;
    p.value = NodePayload::ReconstructionDiagnostic{0.125f, 0.875f};
    ASSERT_TRUE(g.setNodeOutputPayload(n, p));

    const NodePayload* out = g.nodeOutputPayload(n);
    ASSERT_NE(out, nullptr);
    ASSERT_EQ(out->type(), NodePayloadType::ReconstructionDiagnostic);

    const NodePayload::ReconstructionDiagnostic* d = out->reconstructionDiagnostic();
    ASSERT_NE(d, nullptr);
    EXPECT_FLOAT_EQ(d->residual, 0.125f);
    EXPECT_FLOAT_EQ(d->confidence, 0.875f);

    NodePayload::ReconstructionDiagnostic* dMut = g.nodeOutputPayload(n)->reconstructionDiagnostic();
    ASSERT_NE(dMut, nullptr);
    dMut->residual = 0.250f;
    dMut->confidence = 0.750f;

    const NodePayload::ReconstructionDiagnostic* dAfter = g.nodeOutputPayload(n)->reconstructionDiagnostic();
    ASSERT_NE(dAfter, nullptr);
    EXPECT_FLOAT_EQ(dAfter->residual, 0.250f);
    EXPECT_FLOAT_EQ(dAfter->confidence, 0.750f);
}

TEST(EvalGraph, ReconstructionDiagnosticPassesAlphaThresholdHelper) {
    const NodePayload::ReconstructionDiagnostic good{0.125f, 0.875f};
    const NodePayload::ReconstructionDiagnostic badResidual{0.250f, 0.875f};
    const NodePayload::ReconstructionDiagnostic badConfidence{0.125f, 0.750f};

    EXPECT_TRUE(good.passesAlpha());
    EXPECT_FALSE(badResidual.passesAlpha());
    EXPECT_FALSE(badConfidence.passesAlpha());

    // Custom thresholds remain deterministic and explicit at call sites.
    EXPECT_TRUE(badResidual.passesAlpha(/*maxResidual=*/0.300f, /*minConfidence=*/0.800f));
    EXPECT_FALSE(badConfidence.passesAlpha(/*maxResidual=*/0.200f, /*minConfidence=*/0.760f));
}

TEST(EvalGraph, ReconstructionDiagnosticQualityGateCanEmitDeterministicMessage) {
    EvalGraph g;
    NodeId recon = g.addNode(NodeKind::Reconstruction, "recon");
    NodeId proxy = g.addNode(NodeKind::ProxyGeometry, "proxy");
    ASSERT_TRUE(g.connect(recon, proxy));

    g.setComputeCallback([&](NodeComputeContext& context) -> bool {
        if (context.id == recon) {
            if (context.outputPayload) {
                context.outputPayload->value = NodePayload::ReconstructionDiagnostic{0.250f, 0.750f};
            }
            return true;
        }

        if (context.id == proxy) {
            if (context.inputPayloads.empty()) {
                return false;
            }
            const NodePayload* upstream = context.inputPayloads[0].payload;
            const NodePayload::ReconstructionDiagnostic* diag =
                upstream ? upstream->reconstructionDiagnostic() : nullptr;
            if (!diag) {
                return false;
            }
            if (!diag->passesAlpha()) {
                if (context.diagnostics) {
                    context.diagnostics->push_back(
                        "reconstruction quality below alpha threshold: node=" + std::to_string(context.id)
                        + " residual=0.250 confidence=0.750");
                }
            }
            return true;
        }

        return false;
    });

    const EvalReport r = g.evaluate();
    EXPECT_TRUE(r.ok);
    ASSERT_EQ(r.messages.size(), 1u);
    EXPECT_EQ(r.messages[0],
              "reconstruction quality below alpha threshold: node=" + std::to_string(proxy)
              + " residual=0.250 confidence=0.750");
}

// Payload written during an evaluate() pass persists across subsequent clean
// passes (no dirty nodes) and is only overwritten when the node is re-dirtied
// and the callback runs again.
TEST(EvalGraph, PayloadPersistsAcrossCleanPassAndUpdatesOnReDirty) {
    EvalGraph g;
    NodeId src = g.addNode(NodeKind::Constant, "src");
    NodeId dst = g.addNode(NodeKind::Transform, "dst");
    ASSERT_TRUE(g.connect(src, dst));

    // Seed src with a known value before first evaluate.
    NodePayload seed;
    seed.value = 5.0f;
    ASSERT_TRUE(g.setNodeOutputPayload(src, seed));

    int callCount = 0;
    g.setComputeCallback([&](NodeComputeContext& context) -> bool {
        ++callCount;
        if (context.id == dst && context.outputPayload) {
            const NodeInputPayload& inp = context.inputPayloads[0];
            if (inp.payload && inp.payload->scalarF32()) {
                context.outputPayload->value = *inp.payload->scalarF32() * 2.0f;
            }
        }
        return true;
    });

    // First pass: both nodes dirty, callback runs twice.
    const auto r1 = g.evaluate();
    EXPECT_TRUE(r1.ok);
    EXPECT_EQ(r1.dirtyNodes.size(), 2u);
    EXPECT_EQ(callCount, 2);
    {
        const NodePayload* out = g.nodeOutputPayload(dst);
        ASSERT_NE(out, nullptr);
        ASSERT_EQ(out->type(), NodePayloadType::ScalarF32);
        EXPECT_FLOAT_EQ(*out->scalarF32(), 10.0f);
    }

    // Second pass: no dirty nodes — callback must NOT be invoked.
    callCount = 0;
    const auto r2 = g.evaluate();
    EXPECT_TRUE(r2.ok);
    EXPECT_TRUE(r2.dirtyNodes.empty());
    EXPECT_EQ(callCount, 0);
    // Payload must be unchanged after the clean pass.
    {
        const NodePayload* out = g.nodeOutputPayload(dst);
        ASSERT_NE(out, nullptr);
        ASSERT_EQ(out->type(), NodePayloadType::ScalarF32);
        EXPECT_FLOAT_EQ(*out->scalarF32(), 10.0f);
    }

    // Re-dirty src (and therefore dst) and update the seed value.
    NodePayload seed2;
    seed2.value = 7.0f;
    ASSERT_TRUE(g.setNodeOutputPayload(src, seed2));
    g.markDirty(src);

    callCount = 0;
    const auto r3 = g.evaluate();
    EXPECT_TRUE(r3.ok);
    EXPECT_EQ(r3.dirtyNodes.size(), 2u);
    EXPECT_EQ(callCount, 2);

    // dst payload must now reflect the new upstream value.
    {
        const NodePayload* out = g.nodeOutputPayload(dst);
        ASSERT_NE(out, nullptr);
        ASSERT_EQ(out->type(), NodePayloadType::ScalarF32);
        EXPECT_FLOAT_EQ(*out->scalarF32(), 14.0f);
    }
}

TEST(EvalGraph, SetNodeOutputPayloadMarksDownstreamDirty)
{
    EvalGraph g;
    NodeId src = g.addNode(NodeKind::Constant, "src");
    NodeId mid = g.addNode(NodeKind::Geometry, "mid");
    NodeId leaf = g.addNode(NodeKind::Transform, "leaf");
    ASSERT_TRUE(g.connect(src, mid));
    ASSERT_TRUE(g.connect(mid, leaf));

    g.clearDirtyAll();

    NodePayload payload;
    payload.value = 1.0f;
    ASSERT_TRUE(g.setNodeOutputPayload(src, payload));

    EXPECT_TRUE(g.isDirty(src));
    EXPECT_TRUE(g.isDirty(mid));
    EXPECT_TRUE(g.isDirty(leaf));
}

TEST(EvalGraph, SetNodeOutputPayloadRejectsNonFiniteScalarF32)
{
    EvalGraph g;
    NodeId n = g.addNode(NodeKind::Constant, "c");

    // Set a valid float first.
    NodePayload valid;
    valid.value = 2.5f;
    ASSERT_TRUE(g.setNodeOutputPayload(n, valid));
    EXPECT_FLOAT_EQ(*g.nodeOutputPayload(n)->scalarF32(), 2.5f);

    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    const float ninf = -std::numeric_limits<float>::infinity();

    NodePayload bad;
    bad.value = nan;
    EXPECT_FALSE(g.setNodeOutputPayload(n, bad));
    EXPECT_FLOAT_EQ(*g.nodeOutputPayload(n)->scalarF32(), 2.5f);

    bad.value = inf;
    EXPECT_FALSE(g.setNodeOutputPayload(n, bad));
    EXPECT_FLOAT_EQ(*g.nodeOutputPayload(n)->scalarF32(), 2.5f);

    bad.value = ninf;
    EXPECT_FALSE(g.setNodeOutputPayload(n, bad));
    EXPECT_FLOAT_EQ(*g.nodeOutputPayload(n)->scalarF32(), 2.5f);
}

TEST(EvalGraph, ClearNodeOutputPayloadMarksDownstreamDirty)
{
    EvalGraph g;
    NodeId src = g.addNode(NodeKind::Constant, "src");
    NodeId mid = g.addNode(NodeKind::Geometry, "mid");
    NodeId leaf = g.addNode(NodeKind::Transform, "leaf");
    ASSERT_TRUE(g.connect(src, mid));
    ASSERT_TRUE(g.connect(mid, leaf));

    NodePayload payload;
    payload.value = 1.0f;
    ASSERT_TRUE(g.setNodeOutputPayload(src, payload));
    (void)g.evaluate();

    ASSERT_TRUE(g.clearNodeOutputPayload(src));

    EXPECT_TRUE(g.isDirty(src));
    EXPECT_TRUE(g.isDirty(mid));
    EXPECT_TRUE(g.isDirty(leaf));
}

TEST(EvalGraph, MessagesAreDeterministicAndSorted)
{
    // Two identical graphs must produce identical (deterministic) message vectors.
    auto makeGraph = []() {
        EvalGraph g;
        // Create a cycle: a -> b -> a
        const NodeId a = g.addNode(NodeKind::Transform, "a");
        const NodeId b = g.addNode(NodeKind::Transform, "b");
        (void)g.connect(a, b);
        (void)g.connect(b, a);
        return g.evaluate();
    };

    const EvalReport r1 = makeGraph();
    const EvalReport r2 = makeGraph();

    EXPECT_FALSE(r1.ok);
    EXPECT_TRUE(r1.hasCycle);
    EXPECT_EQ(r1.messages, r2.messages);

    // Contract: messages must be lexicographically sorted.
    auto sorted = r1.messages;
    std::sort(sorted.begin(), sorted.end());
    EXPECT_EQ(r1.messages, sorted);
}

TEST(EvalGraph, MultipleWarningMessagesAreLexicographicallySorted)
{
    // A graph with a failing compute callback accumulates messages; they must be sorted.
    EvalGraph g;
    const NodeId n1 = g.addNode(NodeKind::Transform, "alpha");
    const NodeId n2 = g.addNode(NodeKind::Transform, "beta");
    ASSERT_TRUE(g.connect(n1, n2));

    // Callback always fails, causing a diagnostic message to be appended.
    g.setComputeCallback([](NodeComputeContext&) -> bool { return false; });

    const EvalReport r = g.evaluate();
    EXPECT_FALSE(r.ok);
    EXPECT_TRUE(r.executionFailed);
    EXPECT_FALSE(r.messages.empty());

    auto sorted = r.messages;
    std::sort(sorted.begin(), sorted.end());
    EXPECT_EQ(r.messages, sorted);
}
