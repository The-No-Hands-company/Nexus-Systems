#include <gtest/gtest.h>
#include "nexus/scene/NodeScene.h"

using namespace nexus;

// ── Node management ───────────────────────────────────────────────────────────

TEST(NodeScene, EmptySceneHasZeroNodes) {
    NodeScene s;
    EXPECT_EQ(s.nodeCount(), 0u);
}

TEST(NodeScene, AddNodeReturnsStableId) {
    NodeScene s;
    SceneNodeId id = s.addNode("geo", NodeKind::Geometry);
    EXPECT_NE(id, kInvalidSceneNodeId);
    EXPECT_TRUE(s.hasNode(id));
    EXPECT_EQ(s.nodeCount(), 1u);
}

TEST(NodeScene, AddDuplicateNameReturnsInvalidId) {
    NodeScene s;
    s.addNode("geo", NodeKind::Geometry);
    EXPECT_EQ(s.addNode("geo", NodeKind::Transform), kInvalidSceneNodeId);
    EXPECT_EQ(s.nodeCount(), 1u);
}

TEST(NodeScene, NodeByNameReturnsCorrectId) {
    NodeScene s;
    SceneNodeId id = s.addNode("anim", NodeKind::Animation);
    EXPECT_EQ(s.nodeByName("anim"), id);
}

TEST(NodeScene, NodeByNameReturnInvalidForAbsentName) {
    NodeScene s;
    EXPECT_EQ(s.nodeByName("missing"), kInvalidSceneNodeId);
}

TEST(NodeScene, NodeKindAndNameRoundTrip) {
    NodeScene s;
    SceneNodeId id = s.addNode("tx", NodeKind::Transform);
    EXPECT_EQ(s.nodeKind(id), NodeKind::Transform);
    EXPECT_EQ(s.nodeName(id), "tx");
}

TEST(NodeScene, RemoveNodeDecreasesCountAndClearsName) {
    NodeScene s;
    SceneNodeId id = s.addNode("geo", NodeKind::Geometry);
    ASSERT_TRUE(s.removeNode(id));
    EXPECT_EQ(s.nodeCount(), 0u);
    EXPECT_FALSE(s.hasNode(id));
    EXPECT_EQ(s.nodeByName("geo"), kInvalidSceneNodeId);
}

TEST(NodeScene, RemoveUnknownNodeReturnsFalse) {
    NodeScene s;
    EXPECT_FALSE(s.removeNode(kInvalidSceneNodeId));
    EXPECT_FALSE(s.removeNode(999u));
}

TEST(NodeScene, ClearRemovesAllNodesAndNames) {
    NodeScene s;
    s.addNode("a", NodeKind::Constant);
    s.addNode("b", NodeKind::Geometry);
    s.clear();
    EXPECT_EQ(s.nodeCount(), 0u);
    EXPECT_EQ(s.nodeByName("a"), kInvalidSceneNodeId);
    EXPECT_EQ(s.nodeByName("b"), kInvalidSceneNodeId);
}

TEST(NodeScene, RemovedNameCanBeReused) {
    NodeScene s;
    SceneNodeId id1 = s.addNode("node", NodeKind::Constant);
    ASSERT_TRUE(s.removeNode(id1));
    SceneNodeId id2 = s.addNode("node", NodeKind::Geometry);
    EXPECT_NE(id2, kInvalidSceneNodeId);
    EXPECT_NE(id2, id1);
    EXPECT_EQ(s.nodeByName("node"), id2);
}

// ── Dependency edges ──────────────────────────────────────────────────────────

TEST(NodeScene, ConnectAndIsConnected) {
    NodeScene s;
    SceneNodeId a = s.addNode("a", NodeKind::Constant);
    SceneNodeId b = s.addNode("b", NodeKind::Geometry);
    ASSERT_TRUE(s.connect(a, b));
    EXPECT_TRUE(s.isConnected(a, b));
    EXPECT_FALSE(s.isConnected(b, a));
}

TEST(NodeScene, ConnectUnknownNodeReturnsFalse) {
    NodeScene s;
    SceneNodeId a = s.addNode("a", NodeKind::Constant);
    EXPECT_FALSE(s.connect(a, 999u));
    EXPECT_FALSE(s.connect(999u, a));
}

TEST(NodeScene, DisconnectRemovesEdge) {
    NodeScene s;
    SceneNodeId a = s.addNode("a", NodeKind::Constant);
    SceneNodeId b = s.addNode("b", NodeKind::Geometry);
    ASSERT_TRUE(s.connect(a, b));
    ASSERT_TRUE(s.disconnect(a, b));
    EXPECT_FALSE(s.isConnected(a, b));
}

// ── Asset slots ───────────────────────────────────────────────────────────────

TEST(NodeScene, SetAndGetAsset) {
    NodeScene s;
    SceneNodeId id = s.addNode("src", NodeKind::Constant);

    NodePayload p;
    p.value = 42.0f;
    ASSERT_TRUE(s.setAsset(id, p));

    const NodePayload* a = s.asset(id);
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->type(), NodePayloadType::ScalarF32);
    ASSERT_NE(a->scalarF32(), nullptr);
    EXPECT_FLOAT_EQ(*a->scalarF32(), 42.0f);
}

TEST(NodeScene, AssetUnknownNodeReturnsNullptr) {
    NodeScene s;
    EXPECT_EQ(s.asset(999u), nullptr);
}

TEST(NodeScene, SetAssetUnknownNodeReturnsFalse) {
    NodeScene s;
    NodePayload p;
    p.value = 1.0f;
    EXPECT_FALSE(s.setAsset(999u, p));
}

TEST(NodeScene, ClearAssetResetsToNone) {
    NodeScene s;
    SceneNodeId id = s.addNode("n", NodeKind::Geometry);
    NodePayload p;
    p.value = std::string("data");
    ASSERT_TRUE(s.setAsset(id, p));
    ASSERT_TRUE(s.clearAsset(id));
    const NodePayload* a = s.asset(id);
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->type(), NodePayloadType::None);
}

TEST(NodeScene, AssetPersistedAfterRemoveAndReaddWithSameName) {
    NodeScene s;
    SceneNodeId id1 = s.addNode("n", NodeKind::Constant);
    NodePayload p;
    p.value = 7.0f;
    ASSERT_TRUE(s.setAsset(id1, p));
    ASSERT_TRUE(s.removeNode(id1));

    // Re-add same name — must get fresh slot, not stale payload.
    SceneNodeId id2 = s.addNode("n", NodeKind::Constant);
    ASSERT_NE(id2, kInvalidSceneNodeId);
    const NodePayload* a = s.asset(id2);
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->type(), NodePayloadType::None);
}

TEST(NodeScene, SetReconstructionDiagnosticUnknownNodeReturnsFalse) {
    NodeScene s;
    EXPECT_FALSE(
        s.setReconstructionDiagnostic(999u, NodePayload::ReconstructionDiagnostic{0.125f, 0.875f}));
    EXPECT_EQ(s.reconstructionDiagnostic(999u), nullptr);
}

TEST(NodeScene, ReconstructionDiagnosticRoundTripViaConvenienceApi) {
    NodeScene s;
    SceneNodeId id = s.addNode("recon", NodeKind::Reconstruction);

    const NodePayload::ReconstructionDiagnostic diag{0.125f, 0.875f};
    ASSERT_TRUE(s.setReconstructionDiagnostic(id, diag));

    const NodePayload::ReconstructionDiagnostic* got = s.reconstructionDiagnostic(id);
    ASSERT_NE(got, nullptr);
    EXPECT_FLOAT_EQ(got->residual, 0.125f);
    EXPECT_FLOAT_EQ(got->confidence, 0.875f);

    const NodePayload* raw = s.asset(id);
    ASSERT_NE(raw, nullptr);
    EXPECT_EQ(raw->type(), NodePayloadType::ReconstructionDiagnostic);
}

TEST(NodeScene, ReconstructionPassesAlphaReturnsFalseForUnknownAndNonDiagnosticPayloads) {
    NodeScene s;
    EXPECT_FALSE(s.reconstructionPassesAlpha(999u));

    SceneNodeId n = s.addNode("plain", NodeKind::Geometry);
    NodePayload p;
    p.value = 1.0f;
    ASSERT_TRUE(s.setAsset(n, p));
    EXPECT_FALSE(s.reconstructionPassesAlpha(n));
}

TEST(NodeScene, ReconstructionPassesAlphaUsesDefaultAndCustomThresholds) {
    NodeScene s;
    SceneNodeId n = s.addNode("recon", NodeKind::Reconstruction);

    ASSERT_TRUE(s.setReconstructionDiagnostic(n, NodePayload::ReconstructionDiagnostic{0.125f, 0.875f}));
    EXPECT_TRUE(s.reconstructionPassesAlpha(n));

    ASSERT_TRUE(s.setReconstructionDiagnostic(n, NodePayload::ReconstructionDiagnostic{0.250f, 0.750f}));
    EXPECT_FALSE(s.reconstructionPassesAlpha(n));

    // Custom thresholds can intentionally relax or tighten quality gates.
    EXPECT_TRUE(s.reconstructionPassesAlpha(n, /*maxResidual=*/0.300f, /*minConfidence=*/0.700f));
    EXPECT_FALSE(s.reconstructionPassesAlpha(n, /*maxResidual=*/0.240f, /*minConfidence=*/0.760f));
}

TEST(NodeScene, ReconstructionQualitySummaryStrictFormatUnknownNode) {
    NodeScene s;
    EXPECT_EQ(
        s.reconstructionQualitySummary(999u),
        "reconstruction_status=unavailable node=999 reason=unknown_node");
}

TEST(NodeScene, ReconstructionQualitySummaryStrictFormatMissingDiagnostic) {
    NodeScene s;
    SceneNodeId n = s.addNode("plain", NodeKind::Geometry);

    EXPECT_EQ(
        s.reconstructionQualitySummary(n),
        "reconstruction_status=unavailable node=" + std::to_string(n) + " reason=missing_diagnostic");
}

TEST(NodeScene, ReconstructionQualitySummaryStrictFormatPass) {
    NodeScene s;
    SceneNodeId n = s.addNode("recon", NodeKind::Reconstruction);
    ASSERT_TRUE(s.setReconstructionDiagnostic(n, NodePayload::ReconstructionDiagnostic{0.125f, 0.875f}));

    EXPECT_EQ(
        s.reconstructionQualitySummary(n),
        "reconstruction_status=pass node=" + std::to_string(n)
            + " residual=0.125 confidence=0.875 residual_threshold=0.200 confidence_threshold=0.800");
}

TEST(NodeScene, ReconstructionQualitySummaryStrictFormatFail) {
    NodeScene s;
    SceneNodeId n = s.addNode("recon", NodeKind::Reconstruction);
    ASSERT_TRUE(s.setReconstructionDiagnostic(n, NodePayload::ReconstructionDiagnostic{0.250f, 0.750f}));

    EXPECT_EQ(
        s.reconstructionQualitySummary(n),
        "reconstruction_status=fail node=" + std::to_string(n)
            + " residual=0.250 confidence=0.750 residual_threshold=0.200 confidence_threshold=0.800");
}

TEST(NodeScene, ReconstructionQualitySummaryCustomThresholdsAffectStatusAndReportedThresholds) {
    NodeScene s;
    SceneNodeId n = s.addNode("recon", NodeKind::Reconstruction);
    ASSERT_TRUE(s.setReconstructionDiagnostic(n, NodePayload::ReconstructionDiagnostic{0.250f, 0.750f}));

    EXPECT_EQ(
        s.reconstructionQualitySummary(n, /*maxResidual=*/0.300f, /*minConfidence=*/0.700f),
        "reconstruction_status=pass node=" + std::to_string(n)
            + " residual=0.250 confidence=0.750 residual_threshold=0.300 confidence_threshold=0.700");

    EXPECT_EQ(
        s.reconstructionQualitySummary(n, /*maxResidual=*/0.240f, /*minConfidence=*/0.760f),
        "reconstruction_status=fail node=" + std::to_string(n)
            + " residual=0.250 confidence=0.750 residual_threshold=0.240 confidence_threshold=0.760");
}

TEST(NodeScene, ReconstructionQualitySummaryCustomThresholdsPreserveUnavailableReasons) {
    NodeScene s;
    EXPECT_EQ(
        s.reconstructionQualitySummary(999u, /*maxResidual=*/0.300f, /*minConfidence=*/0.700f),
        "reconstruction_status=unavailable node=999 reason=unknown_node");

    SceneNodeId n = s.addNode("plain", NodeKind::Geometry);
    EXPECT_EQ(
        s.reconstructionQualitySummary(n, /*maxResidual=*/0.300f, /*minConfidence=*/0.700f),
        "reconstruction_status=unavailable node=" + std::to_string(n) + " reason=missing_diagnostic");
}

TEST(NodeScene, ReconstructionQualityStateUnknownNode) {
    NodeScene s;
    EXPECT_EQ(
        s.reconstructionQualityState(999u),
        ReconstructionQualityState::UnavailableUnknownNode);
}

TEST(NodeScene, ReconstructionQualityStateMissingDiagnostic) {
    NodeScene s;
    SceneNodeId n = s.addNode("plain", NodeKind::Geometry);
    EXPECT_EQ(
        s.reconstructionQualityState(n),
        ReconstructionQualityState::UnavailableMissingDiagnostic);
}

TEST(NodeScene, ReconstructionQualityStatePassAndFailWithDefaultThresholds) {
    NodeScene s;
    SceneNodeId n = s.addNode("recon", NodeKind::Reconstruction);

    ASSERT_TRUE(s.setReconstructionDiagnostic(n, NodePayload::ReconstructionDiagnostic{0.125f, 0.875f}));
    EXPECT_EQ(s.reconstructionQualityState(n), ReconstructionQualityState::Pass);

    ASSERT_TRUE(s.setReconstructionDiagnostic(n, NodePayload::ReconstructionDiagnostic{0.250f, 0.750f}));
    EXPECT_EQ(s.reconstructionQualityState(n), ReconstructionQualityState::Fail);
}

TEST(NodeScene, ReconstructionQualityStateUsesCustomThresholds) {
    NodeScene s;
    SceneNodeId n = s.addNode("recon", NodeKind::Reconstruction);
    ASSERT_TRUE(s.setReconstructionDiagnostic(n, NodePayload::ReconstructionDiagnostic{0.250f, 0.750f}));

    EXPECT_EQ(
        s.reconstructionQualityState(n, /*maxResidual=*/0.300f, /*minConfidence=*/0.700f),
        ReconstructionQualityState::Pass);
    EXPECT_EQ(
        s.reconstructionQualityState(n, /*maxResidual=*/0.240f, /*minConfidence=*/0.760f),
        ReconstructionQualityState::Fail);
}

TEST(NodeScene, ReconstructionQualityThresholdBundleDefaultParityAcrossApis) {
    NodeScene s;
    SceneNodeId n = s.addNode("recon", NodeKind::Reconstruction);
    ASSERT_TRUE(s.setReconstructionDiagnostic(n, NodePayload::ReconstructionDiagnostic{0.125f, 0.875f}));

    const ReconstructionQualityThresholds t{};
    EXPECT_EQ(s.reconstructionPassesAlpha(n, t), s.reconstructionPassesAlpha(n));
    EXPECT_EQ(s.reconstructionQualityState(n, t), s.reconstructionQualityState(n));
    EXPECT_EQ(s.reconstructionQualitySummary(n, t), s.reconstructionQualitySummary(n));
}

TEST(NodeScene, ReconstructionQualityThresholdBundleCustomParityWithScalarOverloads) {
    NodeScene s;
    SceneNodeId n = s.addNode("recon", NodeKind::Reconstruction);
    ASSERT_TRUE(s.setReconstructionDiagnostic(n, NodePayload::ReconstructionDiagnostic{0.250f, 0.750f}));

    const ReconstructionQualityThresholds t{.maxResidual = 0.300f, .minConfidence = 0.700f};
    EXPECT_EQ(
        s.reconstructionPassesAlpha(n, t),
        s.reconstructionPassesAlpha(n, t.maxResidual, t.minConfidence));
    EXPECT_EQ(
        s.reconstructionQualityState(n, t),
        s.reconstructionQualityState(n, t.maxResidual, t.minConfidence));
    EXPECT_EQ(
        s.reconstructionQualitySummary(n, t),
        s.reconstructionQualitySummary(n, t.maxResidual, t.minConfidence));
}

TEST(NodeScene, ReconstructionQualityThresholdBundleSummaryIsDeterministicAcrossCalls) {
    NodeScene s;
    SceneNodeId n = s.addNode("recon", NodeKind::Reconstruction);
    ASSERT_TRUE(s.setReconstructionDiagnostic(n, NodePayload::ReconstructionDiagnostic{0.250f, 0.750f}));

    const ReconstructionQualityThresholds t{.maxResidual = 0.240f, .minConfidence = 0.760f};
    const std::string first = s.reconstructionQualitySummary(n, t);
    const std::string second = s.reconstructionQualitySummary(n, t);
    EXPECT_EQ(first, second);
    EXPECT_EQ(
        first,
        "reconstruction_status=fail node=" + std::to_string(n)
            + " residual=0.250 confidence=0.750 residual_threshold=0.240 confidence_threshold=0.760");
}

// ── Evaluation via internal EvalGraph ────────────────────────────────────────

TEST(NodeScene, EvaluateEmptySceneSucceeds) {
    NodeScene s;
    const auto r = s.evaluate();
    EXPECT_TRUE(r.ok);
    EXPECT_FALSE(r.hasCycle);
    EXPECT_TRUE(r.evaluationOrder.empty());
}

TEST(NodeScene, EvaluateProducesDependencyFirstOrder) {
    NodeScene s;
    SceneNodeId a = s.addNode("a", NodeKind::Constant);
    SceneNodeId b = s.addNode("b", NodeKind::Transform);
    ASSERT_TRUE(s.connect(a, b));

    const auto r = s.evaluate();
    EXPECT_TRUE(r.ok);
    ASSERT_EQ(r.evaluationOrder.size(), 2u);
    auto posA = std::find(r.evaluationOrder.begin(), r.evaluationOrder.end(), a);
    auto posB = std::find(r.evaluationOrder.begin(), r.evaluationOrder.end(), b);
    EXPECT_LT(posA, posB);
}

TEST(NodeScene, EvaluateWithCycleReportsFailure) {
    NodeScene s;
    SceneNodeId a = s.addNode("a", NodeKind::Geometry);
    SceneNodeId b = s.addNode("b", NodeKind::Geometry);
    ASSERT_TRUE(s.connect(a, b));
    ASSERT_TRUE(s.connect(b, a));
    const auto r = s.evaluate();
    EXPECT_FALSE(r.ok);
    EXPECT_TRUE(r.hasCycle);
}

TEST(NodeScene, CallbackReceivesNodeContextWithCorrectId) {
    NodeScene s;
    SceneNodeId a = s.addNode("a", NodeKind::Constant);
    SceneNodeId b = s.addNode("b", NodeKind::Transform);
    ASSERT_TRUE(s.connect(a, b));

    std::vector<SceneNodeId> fired;
    s.setComputeCallback([&](SceneNodeId id, NodeKind, const std::string&) {
        fired.push_back(id);
        return true;
    });

    const auto r = s.evaluate();
    EXPECT_TRUE(r.ok);
    EXPECT_EQ(fired, r.evaluationOrder);
}

TEST(NodeScene, AssetFlowsThroughCallbackContext) {
    NodeScene s;
    SceneNodeId src = s.addNode("src", NodeKind::Constant);
    SceneNodeId dst = s.addNode("dst", NodeKind::Transform);
    ASSERT_TRUE(s.connect(src, dst));

    NodePayload seed;
    seed.value = 3.0f;
    ASSERT_TRUE(s.setAsset(src, seed));

    s.setComputeCallback([&](NodeComputeContext& ctx) -> bool {
        if (ctx.id == dst && ctx.outputPayload) {
            if (!ctx.inputPayloads.empty() && ctx.inputPayloads[0].payload) {
                const float* v = ctx.inputPayloads[0].payload->scalarF32();
                if (v) ctx.outputPayload->value = *v * 10.0f;
            }
        }
        return true;
    });

    const auto r = s.evaluate();
    EXPECT_TRUE(r.ok);

    const NodePayload* out = s.asset(dst);
    ASSERT_NE(out, nullptr);
    ASSERT_EQ(out->type(), NodePayloadType::ScalarF32);
    EXPECT_FLOAT_EQ(*out->scalarF32(), 30.0f);
}

TEST(NodeScene, EvalGraphAccessorReflectsSceneState) {
    NodeScene s;
    SceneNodeId a = s.addNode("a", NodeKind::Constant);
    EXPECT_TRUE(s.evalGraph().hasNode(a));
    EXPECT_EQ(s.evalGraph().nodeCount(), 1u);
}

// ── Hierarchy ────────────────────────────────────────────────────────────────

TEST(NodeScene, SetParentCreatesTreeRelationshipAndDependencyEdge) {
    NodeScene s;
    SceneNodeId root = s.addNode("root", NodeKind::Constant);
    SceneNodeId child = s.addNode("child", NodeKind::Transform);

    EXPECT_TRUE(s.setParent(child, root));
    EXPECT_EQ(s.parent(child), root);
    const auto ch = s.children(root);
    ASSERT_EQ(ch.size(), 1u);
    EXPECT_EQ(ch[0], child);
    EXPECT_TRUE(s.isConnected(root, child));
}

TEST(NodeScene, SetParentRejectsUnknownIds) {
    NodeScene s;
    SceneNodeId a = s.addNode("a", NodeKind::Constant);
    EXPECT_FALSE(s.setParent(a, kInvalidSceneNodeId));
    EXPECT_FALSE(s.setParent(kInvalidSceneNodeId, a));
    EXPECT_FALSE(s.setParent(kInvalidSceneNodeId, kInvalidSceneNodeId));
}

TEST(NodeScene, SetParentRejectsSelf) {
    NodeScene s;
    SceneNodeId a = s.addNode("a", NodeKind::Constant);
    EXPECT_FALSE(s.setParent(a, a));
}

TEST(NodeScene, SetParentRejectsDirectCycle) {
    NodeScene s;
    SceneNodeId a = s.addNode("a", NodeKind::Constant);
    SceneNodeId b = s.addNode("b", NodeKind::Transform);
    EXPECT_TRUE(s.setParent(b, a));  // a → b
    EXPECT_FALSE(s.setParent(a, b)); // would be cyclic
}

TEST(NodeScene, SetParentRejectsDeepCycle) {
    NodeScene s;
    SceneNodeId a = s.addNode("a", NodeKind::Constant);
    SceneNodeId b = s.addNode("b", NodeKind::Transform);
    SceneNodeId c = s.addNode("c", NodeKind::Transform);
    EXPECT_TRUE(s.setParent(b, a));
    EXPECT_TRUE(s.setParent(c, b));
    EXPECT_FALSE(s.setParent(a, c)); // a is ancestor of c; cycle
}

TEST(NodeScene, SetParentReparentsWhenAlreadyHasParent) {
    NodeScene s;
    SceneNodeId p1 = s.addNode("p1", NodeKind::Constant);
    SceneNodeId p2 = s.addNode("p2", NodeKind::Constant);
    SceneNodeId ch = s.addNode("ch", NodeKind::Transform);

    EXPECT_TRUE(s.setParent(ch, p1));
    EXPECT_TRUE(s.setParent(ch, p2)); // reparent: old edge removed, new edge added

    EXPECT_EQ(s.parent(ch), p2);
    EXPECT_FALSE(s.isConnected(p1, ch));
    EXPECT_TRUE(s.isConnected(p2, ch));
    EXPECT_TRUE(s.children(p1).empty());
    ASSERT_EQ(s.children(p2).size(), 1u);
    EXPECT_EQ(s.children(p2)[0], ch);
}

TEST(NodeScene, ClearParentDisconnectsEdgeAndTree) {
    NodeScene s;
    SceneNodeId root = s.addNode("root", NodeKind::Constant);
    SceneNodeId child = s.addNode("child", NodeKind::Transform);
    ASSERT_TRUE(s.setParent(child, root));

    EXPECT_TRUE(s.clearParent(child));
    EXPECT_EQ(s.parent(child), kInvalidSceneNodeId);
    EXPECT_TRUE(s.children(root).empty());
    EXPECT_FALSE(s.isConnected(root, child));
}

TEST(NodeScene, ClearParentReturnsFalseForRootNode) {
    NodeScene s;
    SceneNodeId a = s.addNode("a", NodeKind::Constant);
    EXPECT_FALSE(s.clearParent(a));
}

TEST(NodeScene, ParentOfRootIsInvalid) {
    NodeScene s;
    SceneNodeId a = s.addNode("a", NodeKind::Constant);
    EXPECT_EQ(s.parent(a), kInvalidSceneNodeId);
}

TEST(NodeScene, ChildrenReturnsEmptyForLeafAndUnknown) {
    NodeScene s;
    SceneNodeId a = s.addNode("a", NodeKind::Constant);
    EXPECT_TRUE(s.children(a).empty());
    EXPECT_TRUE(s.children(kInvalidSceneNodeId).empty());
}

TEST(NodeScene, NodeByPathResolvesSingleSegment) {
    NodeScene s;
    SceneNodeId root = s.addNode("root", NodeKind::Constant);
    EXPECT_EQ(s.nodeByPath("root"), root);
}

TEST(NodeScene, NodeByPathResolvesDeepHierarchy) {
    NodeScene s;
    SceneNodeId root = s.addNode("root", NodeKind::Constant);
    SceneNodeId geo  = s.addNode("geo",  NodeKind::Transform);
    SceneNodeId mesh = s.addNode("mesh", NodeKind::Transform);
    ASSERT_TRUE(s.setParent(geo, root));
    ASSERT_TRUE(s.setParent(mesh, geo));

    EXPECT_EQ(s.nodeByPath("root/geo/mesh"), mesh);
    EXPECT_EQ(s.nodeByPath("root/geo"),      geo);
    EXPECT_EQ(s.nodeByPath("root"),          root);
}

TEST(NodeScene, NodeByPathRejectsWrongAncestry) {
    NodeScene s;
    SceneNodeId root = s.addNode("root", NodeKind::Constant);
    SceneNodeId geo  = s.addNode("geo",  NodeKind::Transform);
    SceneNodeId mesh = s.addNode("mesh", NodeKind::Transform);
    ASSERT_TRUE(s.setParent(geo, root));
    ASSERT_TRUE(s.setParent(mesh, geo));

    // "root/mesh" is wrong — mesh's parent is geo, not root
    EXPECT_EQ(s.nodeByPath("root/mesh"), kInvalidSceneNodeId);
}

TEST(NodeScene, NodeByPathRejectsNonRootFirstSegment) {
    NodeScene s;
    SceneNodeId root = s.addNode("root", NodeKind::Constant);
    SceneNodeId geo  = s.addNode("geo",  NodeKind::Transform);
    ASSERT_TRUE(s.setParent(geo, root));

    // "geo" alone as a path should fail: geo has a parent, so it is not a root
    EXPECT_EQ(s.nodeByPath("geo"), kInvalidSceneNodeId);
}

TEST(NodeScene, NodeByPathRejectsEmptyPath) {
    NodeScene s;
    EXPECT_EQ(s.nodeByPath(""), kInvalidSceneNodeId);
}

TEST(NodeScene, NodeByPathRejectsUnknownSegment) {
    NodeScene s;
    s.addNode("root", NodeKind::Constant);
    EXPECT_EQ(s.nodeByPath("root/missing"), kInvalidSceneNodeId);
}

TEST(NodeScene, NodePathBuildsFromRootToNode) {
    NodeScene s;
    SceneNodeId root = s.addNode("root", NodeKind::Constant);
    SceneNodeId geo  = s.addNode("geo",  NodeKind::Transform);
    SceneNodeId mesh = s.addNode("mesh", NodeKind::Transform);
    ASSERT_TRUE(s.setParent(geo, root));
    ASSERT_TRUE(s.setParent(mesh, geo));

    EXPECT_EQ(s.nodePath(root), "root");
    EXPECT_EQ(s.nodePath(geo),  "root/geo");
    EXPECT_EQ(s.nodePath(mesh), "root/geo/mesh");
}

TEST(NodeScene, NodePathReturnsEmptyForUnknownId) {
    NodeScene s;
    EXPECT_EQ(s.nodePath(kInvalidSceneNodeId), "");
}

TEST(NodeScene, RemoveParentOrphansChildrenAndKeepsThemInScene) {
    NodeScene s;
    SceneNodeId root  = s.addNode("root",  NodeKind::Constant);
    SceneNodeId child = s.addNode("child", NodeKind::Transform);
    ASSERT_TRUE(s.setParent(child, root));

    EXPECT_TRUE(s.removeNode(root));
    EXPECT_FALSE(s.hasNode(root));
    EXPECT_TRUE(s.hasNode(child));
    EXPECT_EQ(s.parent(child), kInvalidSceneNodeId);
    EXPECT_FALSE(s.isConnected(root, child));
}

TEST(NodeScene, RemoveChildUnlinksFromParentChildren) {
    NodeScene s;
    SceneNodeId root  = s.addNode("root",  NodeKind::Constant);
    SceneNodeId child = s.addNode("child", NodeKind::Transform);
    ASSERT_TRUE(s.setParent(child, root));

    EXPECT_TRUE(s.removeNode(child));
    EXPECT_TRUE(s.hasNode(root));
    EXPECT_TRUE(s.children(root).empty());
}

TEST(NodeScene, HierarchyIsClearedBySceneClear) {
    NodeScene s;
    SceneNodeId root  = s.addNode("root",  NodeKind::Constant);
    SceneNodeId child = s.addNode("child", NodeKind::Transform);
    ASSERT_TRUE(s.setParent(child, root));

    s.clear();
    EXPECT_EQ(s.nodeCount(), 0u);
    EXPECT_EQ(s.parent(child), kInvalidSceneNodeId);
    EXPECT_TRUE(s.children(root).empty());
}

TEST(NodeScene, MultipleChildrenPreserveInsertionOrder) {
    NodeScene s;
    SceneNodeId root = s.addNode("root", NodeKind::Constant);
    SceneNodeId c1   = s.addNode("c1",   NodeKind::Transform);
    SceneNodeId c2   = s.addNode("c2",   NodeKind::Transform);
    SceneNodeId c3   = s.addNode("c3",   NodeKind::Transform);
    ASSERT_TRUE(s.setParent(c1, root));
    ASSERT_TRUE(s.setParent(c2, root));
    ASSERT_TRUE(s.setParent(c3, root));

    const auto ch = s.children(root);
    ASSERT_EQ(ch.size(), 3u);
    EXPECT_EQ(ch[0], c1);
    EXPECT_EQ(ch[1], c2);
    EXPECT_EQ(ch[2], c3);
}

TEST(NodeScene, HierarchyAndEvalGraphEvaluateInParentFirstOrder) {
    NodeScene s;
    SceneNodeId root  = s.addNode("root",  NodeKind::Constant);
    SceneNodeId child = s.addNode("child", NodeKind::Transform);
    ASSERT_TRUE(s.setParent(child, root));

    // root writes 5.0; child reads parent payload and doubles it → 10.0
    s.setComputeCallback([&](NodeComputeContext& ctx) -> bool {
        if (ctx.id == root) {
            if (ctx.outputPayload) *ctx.outputPayload = NodePayload{5.0f};
            return true;
        }
        if (ctx.id == child) {
            if (ctx.inputPayloads.empty()) return false;
            const NodePayload* up = ctx.inputPayloads[0].payload;
            if (!up || up->type() != NodePayloadType::ScalarF32) return false;
            if (ctx.outputPayload) *ctx.outputPayload = NodePayload{*up->scalarF32() * 2.0f};
            return true;
        }
        return false;
    });

    s.markDirty(root);
    const auto r = s.evaluate();
    EXPECT_TRUE(r.ok);

    const NodePayload* out = s.asset(child);
    ASSERT_NE(out, nullptr);
    ASSERT_EQ(out->type(), NodePayloadType::ScalarF32);
    EXPECT_FLOAT_EQ(*out->scalarF32(), 10.0f);
}

TEST(NodeScene, ChildComputeFailureDoesNotReDirtyParentSource) {
    NodeScene s;
    SceneNodeId root  = s.addNode("root",  NodeKind::Constant);
    SceneNodeId child = s.addNode("child", NodeKind::Transform);
    ASSERT_TRUE(s.setParent(child, root));

    s.setComputeCallback([&](NodeComputeContext& ctx) -> bool {
        if (ctx.id == root) {
            if (ctx.outputPayload) {
                *ctx.outputPayload = NodePayload{3.0f};
            }
            return true;
        }
        if (ctx.id == child) {
            return false;
        }
        return false;
    });

    s.markDirty(root);
    const EvalReport r = s.evaluate();

    EXPECT_FALSE(r.ok);
    EXPECT_TRUE(r.executionFailed);
    EXPECT_EQ(r.failedNode, child);
    EXPECT_FALSE(s.isDirty(root));
    EXPECT_TRUE(s.isDirty(child));
}
