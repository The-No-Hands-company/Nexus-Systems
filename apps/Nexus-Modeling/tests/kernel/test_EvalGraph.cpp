#include <gtest/gtest.h>
#include "nexus/eval/EvalGraph.h"

#include <algorithm>

using namespace nexus;

// ── Node management ───────────────────────────────────────────────────────────

TEST(EvalGraph, EmptyGraphHasZeroNodes) {
    EvalGraph g;
    EXPECT_EQ(g.nodeCount(), 0u);
}

TEST(EvalGraph, AddNodeIncreasesCount) {
    EvalGraph g;
    g.addNode(NodeKind::Geometry, "geo");
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
    g.addNode(NodeKind::Merge, "m1");
    g.addNode(NodeKind::Merge, "m2");
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
    g.connect(a, b);
    EXPECT_FALSE(g.connect(a, b));
}

TEST(EvalGraph, DisconnectRemovesEdge) {
    EvalGraph g;
    NodeId a = g.addNode(NodeKind::Constant, "c");
    NodeId b = g.addNode(NodeKind::Geometry, "g");
    g.connect(a, b);
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
    g.connect(a, b);
    g.connect(b, c);
    g.removeNode(b);
    EXPECT_FALSE(g.isConnected(a, b));
    EXPECT_FALSE(g.isConnected(b, c));
}

TEST(EvalGraph, EdgeDirectionIsNotSymmetric) {
    EvalGraph g;
    NodeId a = g.addNode(NodeKind::Constant, "c");
    NodeId b = g.addNode(NodeKind::Geometry, "g");
    g.connect(a, b);
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
    g.connect(a, b);
    g.connect(b, c);
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
    g.connect(a, b);
    g.connect(b, c);
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
    g.connect(a, b);
    EXPECT_FALSE(g.hasCycle());
}

TEST(EvalGraph, DirectCycleIsDetected) {
    EvalGraph g;
    NodeId a = g.addNode(NodeKind::Geometry, "a");
    NodeId b = g.addNode(NodeKind::Geometry, "b");
    g.connect(a, b);
    g.connect(b, a);
    EXPECT_TRUE(g.hasCycle());
}

TEST(EvalGraph, LongerCycleIsDetected) {
    EvalGraph g;
    NodeId a = g.addNode(NodeKind::Geometry, "a");
    NodeId b = g.addNode(NodeKind::Transform, "b");
    NodeId c = g.addNode(NodeKind::Merge, "c");
    g.connect(a, b);
    g.connect(b, c);
    g.connect(c, a);
    EXPECT_TRUE(g.hasCycle());
}

TEST(EvalGraph, DisconnectBreaksCycleAndHasCycleReturnsFalse) {
    EvalGraph g;
    NodeId a = g.addNode(NodeKind::Geometry, "a");
    NodeId b = g.addNode(NodeKind::Geometry, "b");
    g.connect(a, b);
    g.connect(b, a);
    ASSERT_TRUE(g.hasCycle());
    g.disconnect(b, a);
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
    g.connect(a, b);
    g.connect(b, c);

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
    g.connect(a, b);

    auto r = g.evaluate();
    EXPECT_EQ(r.dirtyNodes.size(), 2u);
}

TEST(EvalGraph, EvaluateSecondPassHasNoDirtyNodes) {
    EvalGraph g;
    NodeId a = g.addNode(NodeKind::Constant, "c");
    NodeId b = g.addNode(NodeKind::Geometry, "g");
    g.connect(a, b);

    g.evaluate(); // first pass clears dirty
    auto r2 = g.evaluate();
    EXPECT_TRUE(r2.ok);
    EXPECT_TRUE(r2.dirtyNodes.empty());
}

TEST(EvalGraph, EvaluateAfterMarkDirtyReEvaluatesDownstream) {
    EvalGraph g;
    NodeId a = g.addNode(NodeKind::Constant, "c");
    NodeId b = g.addNode(NodeKind::Geometry, "g");
    NodeId c = g.addNode(NodeKind::Transform, "t");
    g.connect(a, b);
    g.connect(b, c);
    g.evaluate();   // first pass — clears all dirty

    g.markDirty(a); // cascades to b and c
    auto r = g.evaluate();
    EXPECT_EQ(r.dirtyNodes.size(), 3u);
}

TEST(EvalGraph, EvaluatePartialDirtyOnlyReEvaluatesDirtyBranch) {
    EvalGraph g;
    NodeId a    = g.addNode(NodeKind::Constant, "c");
    NodeId bClean = g.addNode(NodeKind::Geometry, "clean");
    NodeId cDirty = g.addNode(NodeKind::Animation, "dirty");
    g.connect(a, bClean);
    g.connect(a, cDirty);
    g.evaluate(); // clears all

    g.markDirty(cDirty); // only cDirty and its downstream (none) are dirty
    auto r = g.evaluate();
    ASSERT_EQ(r.dirtyNodes.size(), 1u);
    EXPECT_EQ(r.dirtyNodes[0], cDirty);
}

TEST(EvalGraph, EvaluateWithCycleFailsAndReportsHasCycle) {
    EvalGraph g;
    NodeId a = g.addNode(NodeKind::Geometry, "a");
    NodeId b = g.addNode(NodeKind::Geometry, "b");
    g.connect(a, b);
    g.connect(b, a);

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
        g.connect(a, b);
        g.connect(a, c);
        g.connect(b, d);
        g.connect(c, d);
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
    g.connect(root, left);
    g.connect(root, right);
    g.connect(left, merge);
    g.connect(right, merge);

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
    g.connect(a, b);
    g.connect(b, c);

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
    g.connect(a, b);
    g.connect(b, c);

    g.evaluate(); // clear initial dirty state
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
    g.connect(a, b);
    g.connect(b, c);

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
    g.connect(a, b);
    g.connect(a, c);
    g.connect(b, d);
    g.connect(c, d);

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
    g.connect(a, b);
    g.connect(b, a);

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
    g.connect(a, b);
    g.connect(b, c);

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

TEST(EvalGraph, LegacyCallbackRegistrationRemainsSupported) {
    EvalGraph g;
    NodeId a = g.addNode(NodeKind::Constant, "a");
    NodeId b = g.addNode(NodeKind::Geometry, "b");
    g.connect(a, b);

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
    g.connect(src, dst);

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
