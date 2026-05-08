#include <nexus/animation/AnimationCore.h>
#include <nexus/render/SceneGraph.h>

#include <gtest/gtest.h>

using namespace nexus::animation;
using nexus::render::Mat4;
using nexus::render::Node;
using nexus::render::SceneGraph;
using nexus::render::Vec4;
using AnimTransform = nexus::animation::Transform;

TEST(AnimationCore, TransformTrackEmptyReturnsIdentity)
{
    TransformTrack track;
    const AnimTransform t = track.sample(0.5f);
    EXPECT_FLOAT_EQ(t.translation.x, 0.f);
    EXPECT_FLOAT_EQ(t.translation.y, 0.f);
    EXPECT_FLOAT_EQ(t.translation.z, 0.f);
    EXPECT_FLOAT_EQ(t.scale.x, 1.f);
    EXPECT_FLOAT_EQ(t.scale.y, 1.f);
    EXPECT_FLOAT_EQ(t.scale.z, 1.f);
}

TEST(AnimationCore, TransformTrackInterpolatesTranslation)
{
    TransformTrack track;

    TransformKeyframe k0{};
    k0.timeSec = 0.f;
    k0.value.translation = {0.f, 0.f, 0.f};

    TransformKeyframe k1{};
    k1.timeSec = 1.f;
    k1.value.translation = {10.f, 0.f, 0.f};

    track.setKeyframes({k0, k1});

    const AnimTransform t = track.sample(0.25f);
    EXPECT_NEAR(t.translation.x, 2.5f, 1e-5f);
    EXPECT_NEAR(t.translation.y, 0.f, 1e-5f);
    EXPECT_NEAR(t.translation.z, 0.f, 1e-5f);
}

TEST(AnimationCore, TransformTrackExposesExactKeyframes)
{
    TransformTrack track;

    TransformKeyframe k0{};
    k0.timeSec = 0.1f;
    k0.value.translation = {1.f, 2.f, 3.f};

    TransformKeyframe k1{};
    k1.timeSec = 0.9f;
    k1.value.translation = {4.f, 5.f, 6.f};

    track.setKeyframes({k1, k0});

    ASSERT_EQ(track.keyframeCount(), 2u);
    const TransformKeyframe* first = track.keyframe(0);
    const TransformKeyframe* second = track.keyframe(1);
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);

    EXPECT_FLOAT_EQ(first->timeSec, 0.1f);
    EXPECT_FLOAT_EQ(first->value.translation.x, 1.f);
    EXPECT_FLOAT_EQ(second->timeSec, 0.9f);
    EXPECT_FLOAT_EQ(second->value.translation.z, 6.f);
    EXPECT_EQ(track.keyframe(2), nullptr);
}

TEST(AnimationCore, SkeletonRejectsInvalidParentIndex)
{
    Skeleton skel;
    BoneDesc root{};
    root.name = "root";
    root.parentIndex = -1;
    const int32_t rootIdx = skel.addBone(root);
    ASSERT_EQ(rootIdx, 0);

    BoneDesc invalid{};
    invalid.name = "bad";
    invalid.parentIndex = 42;
    const int32_t invalidIdx = skel.addBone(invalid);
    EXPECT_EQ(invalidIdx, Skeleton::kInvalidBone);
    EXPECT_EQ(skel.boneCount(), 1u);
}

TEST(AnimationCore, SkeletonFindBoneIndexByName)
{
    Skeleton skel;
    BoneDesc root{};
    root.name = "root";
    root.parentIndex = -1;
    ASSERT_EQ(skel.addBone(root), 0);

    BoneDesc child{};
    child.name = "child";
    child.parentIndex = 0;
    ASSERT_EQ(skel.addBone(child), 1);

    EXPECT_EQ(skel.findBoneIndexByName("root"), 0);
    EXPECT_EQ(skel.findBoneIndexByName("child"), 1);
    EXPECT_EQ(skel.findBoneIndexByName("missing"), Skeleton::kInvalidBone);
}

TEST(AnimationCore, PoseComputesParentChildModelMatrices)
{
    Skeleton skel;

    BoneDesc root{};
    root.name = "root";
    root.parentIndex = -1;
    ASSERT_EQ(skel.addBone(root), 0);

    BoneDesc child{};
    child.name = "child";
    child.parentIndex = 0;
    ASSERT_EQ(skel.addBone(child), 1);

    Pose pose(skel.boneCount());

    AnimTransform rootT{};
    rootT.translation = {2.f, 0.f, 0.f};
    pose.setLocalTransform(0, rootT);

    AnimTransform childT{};
    childT.translation = {3.f, 0.f, 0.f};
    pose.setLocalTransform(1, childT);

    pose.computeModelMatrices(skel);

    const auto& mats = pose.modelMatrices();
    ASSERT_EQ(mats.size(), 2u);

    Vec4 origin{0.f, 0.f, 0.f, 1.f};
    Vec4 rootWorld = mats[0] * origin;
    Vec4 childWorld = mats[1] * origin;

    EXPECT_NEAR(rootWorld.x, 2.f, 1e-5f);
    EXPECT_NEAR(childWorld.x, 5.f, 1e-5f);
}

TEST(AnimationCore, ClipSamplesTrackAndFallsBackToBindLocal)
{
    Skeleton skel;

    BoneDesc root{};
    root.name = "root";
    root.parentIndex = -1;
    root.bindLocal.translation = {1.f, 0.f, 0.f};
    ASSERT_EQ(skel.addBone(root), 0);

    BoneDesc child{};
    child.name = "child";
    child.parentIndex = 0;
    child.bindLocal.translation = {4.f, 0.f, 0.f};
    ASSERT_EQ(skel.addBone(child), 1);

    TransformTrack rootTrack;
    TransformKeyframe k0{};
    k0.timeSec = 0.f;
    k0.value.translation = {0.f, 0.f, 0.f};

    TransformKeyframe k1{};
    k1.timeSec = 1.f;
    k1.value.translation = {8.f, 0.f, 0.f};
    rootTrack.setKeyframes({k0, k1});

    AnimationClip clip;
    clip.setDurationSec(1.f);
    clip.setBoneTrack(0, rootTrack);

    Pose pose;
    clip.sampleToPose(0.5f, skel, pose);

    const AnimTransform& rootLocal = pose.localTransform(0);
    const AnimTransform& childLocal = pose.localTransform(1);

    EXPECT_NEAR(rootLocal.translation.x, 4.f, 1e-5f);
    EXPECT_NEAR(childLocal.translation.x, 4.f, 1e-5f);
}

TEST(AnimationCore, ClipLoopWrapModeWrapsTime)
{
    Skeleton skel;
    BoneDesc root{};
    root.name = "root";
    ASSERT_EQ(skel.addBone(root), 0);

    TransformTrack track;
    TransformKeyframe k0{};
    k0.timeSec = 0.f;
    k0.value.translation = {0.f, 0.f, 0.f};

    TransformKeyframe k1{};
    k1.timeSec = 1.f;
    k1.value.translation = {10.f, 0.f, 0.f};
    track.setKeyframes({k0, k1});

    AnimationClip clip;
    clip.setDurationSec(1.f);
    clip.setWrapMode(ClipWrapMode::Loop);
    clip.setBoneTrack(0, track);

    Pose pose;
    clip.sampleToPose(1.25f, skel, pose);

    const AnimTransform& rootLocal = pose.localTransform(0);
    EXPECT_NEAR(rootLocal.translation.x, 2.5f, 1e-5f);
}

TEST(AnimationCore, ClipSampleRateQuantizesSampleTime)
{
    Skeleton skel;
    BoneDesc root{};
    root.name = "root";
    ASSERT_EQ(skel.addBone(root), 0);

    TransformTrack track;
    TransformKeyframe k0{};
    k0.timeSec = 0.f;
    k0.value.translation = {0.f, 0.f, 0.f};

    TransformKeyframe k1{};
    k1.timeSec = 1.f;
    k1.value.translation = {10.f, 0.f, 0.f};
    track.setKeyframes({k0, k1});

    AnimationClip clip;
    clip.setDurationSec(1.f);
    clip.setSampleRateHz(2.f);
    clip.setBoneTrack(0, track);

    Pose pose;
    clip.sampleToPose(0.26f, skel, pose);
    EXPECT_NEAR(pose.localTransform(0).translation.x, 5.f, 1e-5f);
}

TEST(AnimationCore, ClipBlenderSupportsPerBoneWeightMask)
{
    Skeleton skel;

    BoneDesc root{};
    root.name = "root";
    ASSERT_EQ(skel.addBone(root), 0);

    BoneDesc child{};
    child.name = "child";
    child.parentIndex = 0;
    ASSERT_EQ(skel.addBone(child), 1);

    TransformTrack aRoot;
    TransformTrack aChild;
    TransformTrack bRoot;
    TransformTrack bChild;

    TransformKeyframe t0{};
    t0.timeSec = 0.f;

    TransformKeyframe a0 = t0;
    a0.value.translation = {0.f, 0.f, 0.f};
    TransformKeyframe a1 = t0;
    a1.timeSec = 1.f;
    a1.value.translation = {0.f, 0.f, 0.f};
    aRoot.setKeyframes({a0, a1});
    aChild.setKeyframes({a0, a1});

    TransformKeyframe b0 = t0;
    b0.value.translation = {10.f, 0.f, 0.f};
    TransformKeyframe b1 = t0;
    b1.timeSec = 1.f;
    b1.value.translation = {20.f, 0.f, 0.f};
    bRoot.setKeyframes({b0, b0});
    bChild.setKeyframes({b1, b1});

    AnimationClip clipA;
    clipA.setDurationSec(1.f);
    clipA.setBoneTrack(0, aRoot);
    clipA.setBoneTrack(1, aChild);

    AnimationClip clipB;
    clipB.setDurationSec(1.f);
    clipB.setBoneTrack(0, bRoot);
    clipB.setBoneTrack(1, bChild);

    BoneWeightMask mask;
    mask.weights = {1.f, 0.f};

    Pose out;
    ClipBlender::blendToPose(clipA, clipB, 0.5f, 0.5f, 0.5f, skel, mask, out);

    EXPECT_NEAR(out.localTransform(0).translation.x, 5.f, 1e-5f);
    EXPECT_NEAR(out.localTransform(1).translation.x, 0.f, 1e-5f);
}

TEST(AnimationCore, EvaluatorAppliesClipToSceneGraphInBoneOrder)
{
    Skeleton skel;

    BoneDesc root{};
    root.name = "root";
    ASSERT_EQ(skel.addBone(root), 0);

    BoneDesc child{};
    child.name = "child";
    child.parentIndex = 0;
    ASSERT_EQ(skel.addBone(child), 1);

    TransformKeyframe r0{};
    r0.timeSec = 0.f;
    r0.value.translation = {0.f, 0.f, 0.f};
    TransformKeyframe r1{};
    r1.timeSec = 1.f;
    r1.value.translation = {4.f, 0.f, 0.f};

    TransformKeyframe c0{};
    c0.timeSec = 0.f;
    c0.value.translation = {0.f, 0.f, 0.f};
    TransformKeyframe c1{};
    c1.timeSec = 1.f;
    c1.value.translation = {3.f, 0.f, 0.f};

    TransformTrack rootTrack;
    rootTrack.setKeyframes({r0, r1});
    TransformTrack childTrack;
    childTrack.setKeyframes({c0, c1});

    AnimationClip clip;
    clip.setDurationSec(1.f);
    clip.setBoneTrack(0, rootTrack);
    clip.setBoneTrack(1, childTrack);

    SceneGraph scene;
    Node* rootNode = scene.createNode("bone_root");
    ASSERT_NE(rootNode, nullptr);
    Node* childNode = scene.createNode("bone_child", rootNode);
    ASSERT_NE(childNode, nullptr);

    AnimationSceneBinding binding;
    binding.setBoneNode(0, rootNode->id());
    binding.setBoneNode(1, childNode->id());

    Pose out;
    const bool ok = AnimationEvaluator::applyClipToSceneGraph(clip, 1.f, skel, binding, scene, out);
    EXPECT_TRUE(ok);

    EXPECT_NEAR(rootNode->localTransform().translation.x, 4.f, 1e-5f);
    EXPECT_NEAR(childNode->localTransform().translation.x, 3.f, 1e-5f);

    const Mat4 childWorld = childNode->worldMatrix();
    EXPECT_NEAR(childWorld.m[0][3], 7.f, 1e-5f);
}

TEST(AnimationCore, SkinningContractComputesAndPacksJointMatrices)
{
    Skeleton skel;
    BoneDesc root{};
    root.name = "root";
    ASSERT_EQ(skel.addBone(root), 0);

    Pose pose(skel.boneCount());
    AnimTransform t{};
    t.translation = {5.f, 0.f, 0.f};
    pose.setLocalTransform(0, t);
    pose.computeModelMatrices(skel);

    nexus::render::Transform invBindT{};
    invBindT.translation = {-2.f, 0.f, 0.f};
    const Mat4 invBind = invBindT.toMatrix();

    const auto joint = SkinningContract::computeJointMatrices(pose, {invBind});
    ASSERT_EQ(joint.size(), 1u);
    EXPECT_NEAR(joint[0].m[0][3], 3.f, 1e-5f);

    const PackedJointMatrices packed3x4 =
        SkinningContract::packJointMatrices(joint, JointMatrixPackingSchema::Mat3x4RowMajorF32);
    EXPECT_EQ(packed3x4.jointCount, 1u);
    EXPECT_EQ(packed3x4.strideBytes, 12u * sizeof(float));
    ASSERT_EQ(packed3x4.values.size(), 12u);
    EXPECT_NEAR(packed3x4.values[3], 3.f, 1e-5f);

    const PackedJointMatrices packed4x4 =
        SkinningContract::packJointMatrices(joint, JointMatrixPackingSchema::Mat4x4RowMajorF32);
    EXPECT_EQ(packed4x4.strideBytes, 16u * sizeof(float));
    ASSERT_EQ(packed4x4.values.size(), 16u);
    EXPECT_NEAR(packed4x4.values[3], 3.f, 1e-5f);
}

TEST(AnimationCore, ClipStateMachineCrossFadeWindowIsDeterministic)
{
    Skeleton skel;
    BoneDesc root{};
    root.name = "root";
    ASSERT_EQ(skel.addBone(root), 0);

    TransformTrack trackA;
    TransformKeyframe a0{};
    a0.timeSec = 0.f;
    a0.value.translation = {0.f, 0.f, 0.f};
    trackA.setKeyframes({a0, a0});

    TransformTrack trackB;
    TransformKeyframe b0{};
    b0.timeSec = 0.f;
    b0.value.translation = {10.f, 0.f, 0.f};
    trackB.setKeyframes({b0, b0});

    AnimationClip clipA;
    clipA.setDurationSec(1.f);
    clipA.setWrapMode(ClipWrapMode::Loop);
    clipA.setBoneTrack(0, trackA);

    AnimationClip clipB;
    clipB.setDurationSec(1.f);
    clipB.setWrapMode(ClipWrapMode::Loop);
    clipB.setBoneTrack(0, trackB);

    ClipStateMachine sm;
    sm.play(clipA);
    ASSERT_TRUE(sm.requestTransition(clipB, 1.f));

    Pose out;
    sm.sampleToPose(skel, out);
    EXPECT_NEAR(out.localTransform(0).translation.x, 0.f, 1e-5f);

    sm.tick(0.25f);
    sm.sampleToPose(skel, out);
    EXPECT_NEAR(sm.transitionWeight(), 0.25f, 1e-5f);
    EXPECT_NEAR(out.localTransform(0).translation.x, 2.5f, 1e-5f);

    sm.tick(0.25f);
    sm.sampleToPose(skel, out);
    EXPECT_NEAR(sm.transitionWeight(), 0.5f, 1e-5f);
    EXPECT_NEAR(out.localTransform(0).translation.x, 5.f, 1e-5f);

    sm.tick(0.5f);
    sm.sampleToPose(skel, out);
    EXPECT_FALSE(sm.isTransitioning());
    EXPECT_NEAR(out.localTransform(0).translation.x, 10.f, 1e-5f);
}

TEST(AnimationCore, ClipStateMachineRejectsInterruptionWhenPolicyIsReject)
{
    Skeleton skel;
    BoneDesc root{};
    root.name = "root";
    ASSERT_EQ(skel.addBone(root), 0);

    auto makeConstantClip = [](float x) {
        TransformTrack track;
        TransformKeyframe k{};
        k.timeSec = 0.f;
        k.value.translation = {x, 0.f, 0.f};
        track.setKeyframes({k, k});

        AnimationClip clip;
        clip.setDurationSec(1.f);
        clip.setWrapMode(ClipWrapMode::Loop);
        clip.setBoneTrack(0, track);
        return clip;
    };

    const AnimationClip clipA = makeConstantClip(0.f);
    const AnimationClip clipB = makeConstantClip(10.f);
    const AnimationClip clipC = makeConstantClip(20.f);

    ClipStateMachine sm;
    sm.setInterruptionPolicy(TransitionInterruptionPolicy::RejectNewTransition);
    sm.play(clipA);
    ASSERT_TRUE(sm.requestTransition(clipB, 1.f));

    sm.tick(0.25f);
    EXPECT_FALSE(sm.requestTransition(clipC, 0.5f));

    sm.tick(0.75f);
    Pose out;
    sm.sampleToPose(skel, out);
    EXPECT_NEAR(out.localTransform(0).translation.x, 10.f, 1e-5f);
}

TEST(AnimationCore, ClipStateMachineQueuesLastInterruptionWhenPolicyIsQueue)
{
    Skeleton skel;
    BoneDesc root{};
    root.name = "root";
    ASSERT_EQ(skel.addBone(root), 0);

    auto makeConstantClip = [](float x) {
        TransformTrack track;
        TransformKeyframe k{};
        k.timeSec = 0.f;
        k.value.translation = {x, 0.f, 0.f};
        track.setKeyframes({k, k});

        AnimationClip clip;
        clip.setDurationSec(1.f);
        clip.setWrapMode(ClipWrapMode::Loop);
        clip.setBoneTrack(0, track);
        return clip;
    };

    const AnimationClip clipA = makeConstantClip(0.f);
    const AnimationClip clipB = makeConstantClip(10.f);
    const AnimationClip clipC = makeConstantClip(20.f);

    ClipStateMachine sm;
    sm.setInterruptionPolicy(TransitionInterruptionPolicy::QueueLastTransition);
    sm.play(clipA);
    ASSERT_TRUE(sm.requestTransition(clipB, 1.f));

    sm.tick(0.25f);
    ASSERT_TRUE(sm.requestTransition(clipC, 0.5f));

    // Completes A->B and starts queued B->C at weight 0.
    sm.tick(0.75f);
    Pose out;
    sm.sampleToPose(skel, out);
    EXPECT_TRUE(sm.isTransitioning());
    EXPECT_NEAR(out.localTransform(0).translation.x, 10.f, 1e-5f);

    sm.tick(0.25f);
    sm.sampleToPose(skel, out);
    EXPECT_NEAR(out.localTransform(0).translation.x, 15.f, 1e-5f);

    sm.tick(0.25f);
    sm.sampleToPose(skel, out);
    EXPECT_FALSE(sm.isTransitioning());
    EXPECT_NEAR(out.localTransform(0).translation.x, 20.f, 1e-5f);
}

TEST(AnimationCore, ClipStateMachineTimePredicateRuleTriggersTransition)
{
    Skeleton skel;
    BoneDesc root{};
    root.name = "root";
    ASSERT_EQ(skel.addBone(root), 0);

    auto makeConstantClip = [](float x) {
        TransformTrack track;
        TransformKeyframe k{};
        k.timeSec = 0.f;
        k.value.translation = {x, 0.f, 0.f};
        track.setKeyframes({k, k});

        AnimationClip clip;
        clip.setDurationSec(2.f);
        clip.setWrapMode(ClipWrapMode::Loop);
        clip.setBoneTrack(0, track);
        return clip;
    };

    const AnimationClip clipA = makeConstantClip(0.f);
    const AnimationClip clipB = makeConstantClip(10.f);

    ClipStateMachine sm;
    sm.play(clipA);
    sm.addTransitionRule({
        .fromClip = &clipA,
        .toClip = &clipB,
        .fadeWindowSec = 0.5f,
        .mask = {},
        .minSourceTimeSec = 0.5f,
        .eventName = "",
    });

    Pose out;
    sm.tick(0.25f);
    sm.sampleToPose(skel, out);
    EXPECT_FALSE(sm.isTransitioning());
    EXPECT_NEAR(out.localTransform(0).translation.x, 0.f, 1e-5f);

    sm.tick(0.25f);
    sm.sampleToPose(skel, out);
    EXPECT_TRUE(sm.isTransitioning());
    EXPECT_NEAR(out.localTransform(0).translation.x, 0.f, 1e-5f);

    sm.tick(0.25f);
    sm.sampleToPose(skel, out);
    EXPECT_NEAR(out.localTransform(0).translation.x, 5.f, 1e-5f);

    sm.tick(0.25f);
    sm.sampleToPose(skel, out);
    EXPECT_FALSE(sm.isTransitioning());
    EXPECT_NEAR(out.localTransform(0).translation.x, 10.f, 1e-5f);
}

TEST(AnimationCore, ClipStateMachineEventPredicateRuleTriggersTransition)
{
    Skeleton skel;
    BoneDesc root{};
    root.name = "root";
    ASSERT_EQ(skel.addBone(root), 0);

    auto makeConstantClip = [](float x) {
        TransformTrack track;
        TransformKeyframe k{};
        k.timeSec = 0.f;
        k.value.translation = {x, 0.f, 0.f};
        track.setKeyframes({k, k});

        AnimationClip clip;
        clip.setDurationSec(2.f);
        clip.setWrapMode(ClipWrapMode::Loop);
        clip.setBoneTrack(0, track);
        return clip;
    };

    const AnimationClip clipA = makeConstantClip(0.f);
    const AnimationClip clipB = makeConstantClip(10.f);

    ClipStateMachine sm;
    sm.play(clipA);
    sm.addTransitionRule({
        .fromClip = &clipA,
        .toClip = &clipB,
        .fadeWindowSec = 1.f,
        .mask = {},
        .minSourceTimeSec = -1.f,
        .eventName = "jump",
    });

    Pose out;
    sm.tick(1.f);
    sm.sampleToPose(skel, out);
    EXPECT_FALSE(sm.isTransitioning());
    EXPECT_NEAR(out.localTransform(0).translation.x, 0.f, 1e-5f);

    sm.pushEvent("jump");
    sm.tick(0.f);
    sm.sampleToPose(skel, out);
    EXPECT_TRUE(sm.isTransitioning());
    EXPECT_NEAR(out.localTransform(0).translation.x, 0.f, 1e-5f);

    sm.tick(0.5f);
    sm.sampleToPose(skel, out);
    EXPECT_NEAR(out.localTransform(0).translation.x, 5.f, 1e-5f);

    sm.tick(0.5f);
    sm.sampleToPose(skel, out);
    EXPECT_FALSE(sm.isTransitioning());
    EXPECT_NEAR(out.localTransform(0).translation.x, 10.f, 1e-5f);
}

TEST(AnimationCore, ClipStateMachineTimeAndEventPredicatesCanBeCombined)
{
    Skeleton skel;
    BoneDesc root{};
    root.name = "root";
    ASSERT_EQ(skel.addBone(root), 0);

    auto makeConstantClip = [](float x) {
        TransformTrack track;
        TransformKeyframe k{};
        k.timeSec = 0.f;
        k.value.translation = {x, 0.f, 0.f};
        track.setKeyframes({k, k});

        AnimationClip clip;
        clip.setDurationSec(2.f);
        clip.setWrapMode(ClipWrapMode::Loop);
        clip.setBoneTrack(0, track);
        return clip;
    };

    const AnimationClip clipA = makeConstantClip(0.f);
    const AnimationClip clipB = makeConstantClip(10.f);

    ClipStateMachine sm;
    sm.play(clipA);
    sm.addTransitionRule({
        .fromClip = &clipA,
        .toClip = &clipB,
        .fadeWindowSec = 0.5f,
        .mask = {},
        .minSourceTimeSec = 0.5f,
        .eventName = "attack",
    });

    Pose out;
    sm.pushEvent("attack");
    sm.tick(0.25f);
    sm.sampleToPose(skel, out);
    EXPECT_FALSE(sm.isTransitioning());

    sm.tick(0.25f);
    sm.sampleToPose(skel, out);
    EXPECT_TRUE(sm.isTransitioning());
}

// ─────────────────────────────────────────────────────────────────────────────
//  AnimationStateGraph tests
// ─────────────────────────────────────────────────────────────────────────────

namespace {

Skeleton makeSingleBoneSkeleton()
{
    Skeleton skel;
    BoneDesc root{};
    root.name = "root";
    [[maybe_unused]] const int32_t rootIndex = skel.addBone(root);
    return skel;
}

AnimationClip makeConstantStateClip(float x)
{
    TransformTrack track;
    TransformKeyframe k{};
    k.timeSec = 0.f;
    k.value.translation = {x, 0.f, 0.f};
    track.setKeyframes({k, k});

    AnimationClip clip;
    clip.setDurationSec(2.f);
    clip.setWrapMode(ClipWrapMode::Loop);
    clip.setBoneTrack(0, track);
    return clip;
}

} // namespace

TEST(AnimationCore, StateGraphPlayByNameStartsCorrectClip)
{
    const Skeleton skel = makeSingleBoneSkeleton();
    const AnimationClip idle = makeConstantStateClip(0.f);
    const AnimationClip walk = makeConstantStateClip(5.f);

    AnimationStateGraph graph;
    ASSERT_TRUE(graph.addState({"idle", &idle}));
    ASSERT_TRUE(graph.addState({"walk", &walk}));

    ASSERT_TRUE(graph.play("walk"));
    EXPECT_EQ(graph.currentStateId(), "walk");

    Pose out;
    graph.sampleToPose(skel, out);
    EXPECT_NEAR(out.localTransform(0).translation.x, 5.f, 1e-5f);
}

TEST(AnimationCore, StateGraphRejectsUnknownStateInPlay)
{
    const AnimationClip idle = makeConstantStateClip(0.f);

    AnimationStateGraph graph;
    ASSERT_TRUE(graph.addState({"idle", &idle}));

    EXPECT_FALSE(graph.play("nonexistent"));
    EXPECT_TRUE(graph.currentStateId().empty());
}

TEST(AnimationCore, StateGraphTransitionRuleFiresByStateId)
{
    const Skeleton skel = makeSingleBoneSkeleton();
    const AnimationClip idle = makeConstantStateClip(0.f);
    const AnimationClip walk = makeConstantStateClip(10.f);

    AnimationStateGraph graph;
    ASSERT_TRUE(graph.addState({"idle", &idle}));
    ASSERT_TRUE(graph.addState({"walk", &walk}));
    ASSERT_TRUE(graph.addTransitionRule({
        .fromStateId = "idle",
        .toStateId = "walk",
        .fadeWindowSec = 0.f,
        .mask = {},
        .minSourceTimeSec = 0.5f,
        .eventName = "",
    }));

    ASSERT_TRUE(graph.play("idle"));

    // Before threshold: no transition.
    graph.tick(0.25f);
    EXPECT_FALSE(graph.isTransitioning());
    EXPECT_EQ(graph.currentStateId(), "idle");

    // Past threshold: instant transition (fadeWindowSec=0).
    graph.tick(0.25f);
    EXPECT_EQ(graph.currentStateId(), "walk");

    Pose out;
    graph.sampleToPose(skel, out);
    EXPECT_NEAR(out.localTransform(0).translation.x, 10.f, 1e-5f);
}

TEST(AnimationCore, StateGraphEventRuleTriggersTransition)
{
    const Skeleton skel = makeSingleBoneSkeleton();
    const AnimationClip idle = makeConstantStateClip(0.f);
    const AnimationClip attack = makeConstantStateClip(7.f);

    AnimationStateGraph graph;
    ASSERT_TRUE(graph.addState({"idle", &idle}));
    ASSERT_TRUE(graph.addState({"attack", &attack}));
    ASSERT_TRUE(graph.addTransitionRule({
        .fromStateId = "idle",
        .toStateId = "attack",
        .fadeWindowSec = 0.f,
        .mask = {},
        .minSourceTimeSec = -1.f,
        .eventName = "attack",
    }));

    ASSERT_TRUE(graph.play("idle"));

    // No event yet: stays in idle.
    graph.tick(0.1f);
    EXPECT_EQ(graph.currentStateId(), "idle");

    // Push event and tick: transitions to attack.
    graph.pushEvent("attack");
    graph.tick(0.1f);
    EXPECT_EQ(graph.currentStateId(), "attack");
}

TEST(AnimationCore, StateGraphRulesAreScopedToSourceState)
{
    const Skeleton skel = makeSingleBoneSkeleton();
    const AnimationClip idle = makeConstantStateClip(0.f);
    const AnimationClip walk = makeConstantStateClip(5.f);
    const AnimationClip run  = makeConstantStateClip(10.f);

    AnimationStateGraph graph;
    ASSERT_TRUE(graph.addState({"idle", &idle}));
    ASSERT_TRUE(graph.addState({"walk", &walk}));
    ASSERT_TRUE(graph.addState({"run", &run}));

    // Rule only on "walk" -> "run"; should NOT fire when in "idle".
    ASSERT_TRUE(graph.addTransitionRule({
        .fromStateId = "walk",
        .toStateId = "run",
        .fadeWindowSec = 0.f,
        .mask = {},
        .minSourceTimeSec = 0.1f,
        .eventName = "",
    }));

    ASSERT_TRUE(graph.play("idle"));
    graph.tick(0.5f);

    // Rule fromStateId is "walk", so must not fire while in "idle".
    EXPECT_EQ(graph.currentStateId(), "idle");
}

TEST(AnimationCore, StateGraphCurrentStateIdPromotedAfterTransition)
{
    const Skeleton skel = makeSingleBoneSkeleton();
    const AnimationClip idle = makeConstantStateClip(0.f);
    const AnimationClip walk = makeConstantStateClip(10.f);

    AnimationStateGraph graph;
    ASSERT_TRUE(graph.addState({"idle", &idle}));
    ASSERT_TRUE(graph.addState({"walk", &walk}));
    ASSERT_TRUE(graph.addTransitionRule({
        .fromStateId = "idle",
        .toStateId = "walk",
        .fadeWindowSec = 0.5f,
        .mask = {},
        .minSourceTimeSec = -1.f,
        .eventName = "go",
    }));

    ASSERT_TRUE(graph.play("idle"));

    graph.pushEvent("go");
    graph.tick(0.1f);

    // Transition started; currentStateId stays "idle" until cross-fade completes.
    EXPECT_TRUE(graph.isTransitioning());
    EXPECT_EQ(graph.currentStateId(), "idle");

    // Tick past the fade window.
    graph.tick(0.5f);
    EXPECT_FALSE(graph.isTransitioning());
    EXPECT_EQ(graph.currentStateId(), "walk");
}
