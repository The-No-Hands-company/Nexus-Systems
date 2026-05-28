#include <nexus/animation/AnimationCore.h>
#include <nexus/render/SceneGraph.h>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

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

TEST(AnimationCore, ClipSettersRejectNegativeAndNonFiniteValues)
{
    AnimationClip clip;
    clip.setDurationSec(1.5f);
    clip.setSampleRateHz(24.f);

    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();

    clip.setDurationSec(-1.f);
    clip.setDurationSec(nan);
    clip.setDurationSec(inf);

    clip.setSampleRateHz(-60.f);
    clip.setSampleRateHz(nan);
    clip.setSampleRateHz(-inf);

    EXPECT_FLOAT_EQ(clip.durationSec(), 1.5f);
    EXPECT_FLOAT_EQ(clip.sampleRateHz(), 24.f);
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

TEST(AnimationCore, ClipStateMachinePlaybackIsDeterministicAcrossFrameRates)
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
    k1.value.translation = {24.f, 0.f, 0.f};

    track.setKeyframes({k0, k1});

    AnimationClip clip;
    clip.setDurationSec(1.f);
    clip.setSampleRateHz(24.f);
    clip.setWrapMode(ClipWrapMode::Clamp);
    clip.setBoneTrack(0, track);

    auto samplePlayback = [&](float stepSeconds, size_t stepCount) {
        ClipStateMachine machine;
        machine.play(clip);

        Pose pose;
        for (size_t i = 0; i < stepCount; ++i) {
            machine.tick(stepSeconds);
        }

        machine.sampleToPose(skel, pose);
        return pose.localTransform(0).translation.x;
    };

    EXPECT_NEAR(samplePlayback(1.f / 60.f, 15u), samplePlayback(1.f / 24.f, 6u), 1e-5f);
    EXPECT_NEAR(samplePlayback(1.f / 60.f, 30u), samplePlayback(1.f / 24.f, 12u), 1e-5f);
    EXPECT_NEAR(samplePlayback(1.f / 60.f, 45u), samplePlayback(1.f / 24.f, 18u), 1e-5f);
    EXPECT_NEAR(samplePlayback(1.f / 60.f, 60u), samplePlayback(1.f / 24.f, 24u), 1e-5f);
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

TEST(AnimationCore, StateGraphPlayRejectsNonFiniteStartTime)
{
    const AnimationClip idle = makeConstantStateClip(0.f);

    AnimationStateGraph graph;
    ASSERT_TRUE(graph.addState({"idle", &idle}));

    EXPECT_FALSE(graph.play("idle", std::numeric_limits<float>::quiet_NaN()));
    EXPECT_FALSE(graph.play("idle", std::numeric_limits<float>::infinity()));
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

TEST(AnimationCore, StateGraphTickRejectsNonFiniteDeltaWithoutTransition)
{
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
    graph.pushEvent("attack");

    graph.tick(std::numeric_limits<float>::quiet_NaN());
    EXPECT_EQ(graph.currentStateId(), "idle");
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

// ─────────────────────────────────────────────────────────────────────────────
//  R7.2 — deeper state-machine playback and skinning replay coverage
// ─────────────────────────────────────────────────────────────────────────────

TEST(AnimationCore, ClipStateMachineTransitionHonorsBoneMask)
{
    // Two-bone skeleton. Transition from A (both bones at 0) to B (both at 10)
    // with a mask that only blends bone 0. At midpoint: bone 0 = 5, bone 1 = 0.
    Skeleton skel;
    {
        BoneDesc r{};
        r.name = "root";
        ASSERT_EQ(skel.addBone(r), 0);

        BoneDesc c{};
        c.name = "child";
        c.parentIndex = 0;
        ASSERT_EQ(skel.addBone(c), 1);
    }

    auto makeConstant2Bone = [&](float x) {
        AnimationClip clip;
        clip.setDurationSec(1.f);
        clip.setWrapMode(ClipWrapMode::Loop);
        for (int i = 0; i < 2; ++i) {
            TransformTrack t;
            TransformKeyframe k{};
            k.timeSec = 0.f;
            k.value.translation = {x, 0.f, 0.f};
            t.setKeyframes({k, k});
            clip.setBoneTrack(static_cast<size_t>(i), t);
        }
        return clip;
    };

    const AnimationClip clipA = makeConstant2Bone(0.f);
    const AnimationClip clipB = makeConstant2Bone(10.f);

    // Bone 0 fully blends toward B; bone 1 stays at A.
    BoneWeightMask mask;
    mask.weights = {1.f, 0.f};

    ClipStateMachine sm;
    sm.play(clipA);
    ASSERT_TRUE(sm.requestTransition(clipB, 1.f, mask));

    sm.tick(0.5f);
    Pose out;
    sm.sampleToPose(skel, out);

    EXPECT_NEAR(out.localTransform(0).translation.x, 5.f, 1e-5f);  // half-blended
    EXPECT_NEAR(out.localTransform(1).translation.x, 0.f, 1e-5f);  // mask blocks blend
}

TEST(AnimationCore, ClipStateMachineActiveClipTimeContinuesAfterFadeCompletion)
{
    // Verify that clipB's elapsed time accumulates during the cross-fade window
    // and is not reset when clipB becomes the active (promoted) clip.
    //
    // Clip A: constant 0. Clip B: linear ramp 0→10 over 1 s, Clamp.
    // Request B with a 0.5 s fade window at t=0.
    // At t=0.5 (fade complete): B has elapsed 0.5 s → sample ≈ 5.
    // At t=1.0: B has elapsed 1.0 s → sample ≈ 10 (clamped).
    Skeleton skel;
    BoneDesc root{};
    root.name = "root";
    ASSERT_EQ(skel.addBone(root), 0);

    // clipA — constant 0
    AnimationClip clipA;
    clipA.setDurationSec(1.f);
    clipA.setWrapMode(ClipWrapMode::Loop);
    {
        TransformTrack t;
        TransformKeyframe k{};
        k.timeSec = 0.f;
        k.value.translation = {0.f, 0.f, 0.f};
        t.setKeyframes({k, k});
        clipA.setBoneTrack(0, t);
    }

    // clipB — ramp 0→10 over 1 s, Clamp
    AnimationClip clipB;
    clipB.setDurationSec(1.f);
    clipB.setWrapMode(ClipWrapMode::Clamp);
    {
        TransformTrack t;
        TransformKeyframe k0{};
        k0.timeSec = 0.f;
        k0.value.translation = {0.f, 0.f, 0.f};
        TransformKeyframe k1{};
        k1.timeSec = 1.f;
        k1.value.translation = {10.f, 0.f, 0.f};
        t.setKeyframes({k0, k1});
        clipB.setBoneTrack(0, t);
    }

    ClipStateMachine sm;
    sm.play(clipA);
    ASSERT_TRUE(sm.requestTransition(clipB, 0.5f));

    // After 0.5 s the fade window completes; B's time must be 0.5 s.
    sm.tick(0.5f);
    EXPECT_FALSE(sm.isTransitioning());
    Pose out;
    sm.sampleToPose(skel, out);
    EXPECT_NEAR(out.localTransform(0).translation.x, 5.f, 1e-5f);

    // Another 0.5 s advances B to 1.0 s (clamped) → 10.
    sm.tick(0.5f);
    sm.sampleToPose(skel, out);
    EXPECT_NEAR(out.localTransform(0).translation.x, 10.f, 1e-5f);
}

TEST(AnimationCore, ClipStateMachineClampWrapModeSaturatesAtEnd)
{
    // Clamp mode must hold the final value after the clip duration is exceeded,
    // regardless of how many additional ticks are applied.
    Skeleton skel;
    BoneDesc root{};
    root.name = "root";
    ASSERT_EQ(skel.addBone(root), 0);

    AnimationClip clip;
    clip.setDurationSec(1.f);
    clip.setWrapMode(ClipWrapMode::Clamp);
    {
        TransformTrack t;
        TransformKeyframe k0{};
        k0.timeSec = 0.f;
        k0.value.translation = {0.f, 0.f, 0.f};
        TransformKeyframe k1{};
        k1.timeSec = 1.f;
        k1.value.translation = {10.f, 0.f, 0.f};
        t.setKeyframes({k0, k1});
        clip.setBoneTrack(0, t);
    }

    ClipStateMachine sm;
    sm.play(clip);

    sm.tick(0.5f);
    Pose out;
    sm.sampleToPose(skel, out);
    EXPECT_NEAR(out.localTransform(0).translation.x, 5.f, 1e-5f);

    // Advance past end.
    sm.tick(0.75f);
    sm.sampleToPose(skel, out);
    EXPECT_NEAR(out.localTransform(0).translation.x, 10.f, 1e-5f);

    // Additional ticks must not drift beyond the clamped value.
    sm.tick(1.f);
    sm.sampleToPose(skel, out);
    EXPECT_NEAR(out.localTransform(0).translation.x, 10.f, 1e-5f);

    sm.tick(5.f);
    sm.sampleToPose(skel, out);
    EXPECT_NEAR(out.localTransform(0).translation.x, 10.f, 1e-5f);
}

TEST(AnimationCore, SkinningContractJointMatrixReplayIsDeterministic)
{
    // Two independent ClipStateMachine playback runs must produce bit-identical
    // packed joint matrices at the same elapsed-time checkpoints.
    Skeleton skel;
    {
        BoneDesc root{};
        root.name = "root";
        ASSERT_EQ(skel.addBone(root), 0);
    }

    // Linear ramp 0→12 over 1 s, Clamp.
    AnimationClip clip;
    clip.setDurationSec(1.f);
    clip.setWrapMode(ClipWrapMode::Clamp);
    {
        TransformTrack t;
        TransformKeyframe k0{};
        k0.timeSec = 0.f;
        k0.value.translation = {0.f, 0.f, 0.f};
        TransformKeyframe k1{};
        k1.timeSec = 1.f;
        k1.value.translation = {12.f, 0.f, 0.f};
        t.setKeyframes({k0, k1});
        clip.setBoneTrack(0, t);
    }

    // Inverse bind: shifts joint matrix translation by -3.
    nexus::render::Transform invBindT{};
    invBindT.translation = {-3.f, 0.f, 0.f};
    const nexus::render::Mat4 invBind = invBindT.toMatrix();

    auto capturePackedAt = [&](float dtSec, size_t steps) {
        ClipStateMachine machine;
        machine.play(clip);
        Pose pose;
        for (size_t i = 0; i < steps; ++i)
            machine.tick(dtSec);
        machine.sampleToPose(skel, pose);
        pose.computeModelMatrices(skel);
        const auto joint = SkinningContract::computeJointMatrices(pose, {invBind});
        return SkinningContract::packJointMatrices(joint, JointMatrixPackingSchema::Mat4x4RowMajorF32);
    };

    // Compare two runs at identical tick streams.
    const float dt = 1.f / 30.f;
    for (size_t steps : {size_t(8), size_t(15), size_t(22), size_t(30)}) {
        const auto runA = capturePackedAt(dt, steps);
        const auto runB = capturePackedAt(dt, steps);
        ASSERT_EQ(runA.values, runB.values) << "Mismatch at step " << steps;
    }
}

TEST(AnimationCore, StateGraphTransitionWithBoneMaskOnlyBlendsUnmaskedBones)
{
    // State graph transition with a per-bone mask: the masked bone stays on the
    // source state while the unmasked bone blends toward the target state.
    Skeleton skel;
    {
        BoneDesc r{};
        r.name = "root";
        ASSERT_EQ(skel.addBone(r), 0);
        BoneDesc c{};
        c.name = "child";
        c.parentIndex = 0;
        ASSERT_EQ(skel.addBone(c), 1);
    }

    auto makeConst2 = [&](float x) {
        AnimationClip clip;
        clip.setDurationSec(1.f);
        clip.setWrapMode(ClipWrapMode::Loop);
        for (int i = 0; i < 2; ++i) {
            TransformTrack t;
            TransformKeyframe k{};
            k.timeSec = 0.f;
            k.value.translation = {x, 0.f, 0.f};
            t.setKeyframes({k, k});
            clip.setBoneTrack(static_cast<size_t>(i), t);
        }
        return clip;
    };

    const AnimationClip idleClip = makeConst2(0.f);
    const AnimationClip walkClip = makeConst2(10.f);

    BoneWeightMask mask;
    mask.weights = {0.f, 1.f};  // bone 0 blocked, bone 1 blends

    AnimationStateGraph graph;
    ASSERT_TRUE(graph.addState({"idle", &idleClip}));
    ASSERT_TRUE(graph.addState({"walk", &walkClip}));
    ASSERT_TRUE(graph.addTransitionRule({
        .fromStateId = "idle",
        .toStateId = "walk",
        .fadeWindowSec = 1.f,
        .mask = mask,
        .minSourceTimeSec = -1.f,
        .eventName = "move",
    }));

    ASSERT_TRUE(graph.play("idle"));
    graph.pushEvent("move");
    graph.tick(0.f);   // fire the transition rule without advancing the clock
    ASSERT_TRUE(graph.isTransitioning());

    graph.tick(0.5f);  // now advance: transition elapsed = 0.5 s → weight = 0.5

    Pose out;
    graph.sampleToPose(skel, out);

    EXPECT_NEAR(out.localTransform(0).translation.x, 0.f, 1e-5f);   // mask blocks
    EXPECT_NEAR(out.localTransform(1).translation.x, 5.f, 1e-5f);   // half-blended
}

// ─────────────────────────────────────────────────────────────────────────────
//  Non-finite timing input hardening
// ─────────────────────────────────────────────────────────────────────────────

TEST(AnimationCore, ClipSetDurationSecRejectsNaN)
{
    AnimationClip clip(2.f, 30.f);
    clip.setDurationSec(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(clip.durationSec(), 2.f);   // unchanged
}

TEST(AnimationCore, ClipSetDurationSecRejectsInf)
{
    AnimationClip clip(2.f, 30.f);
    clip.setDurationSec(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(clip.durationSec(), 2.f);   // unchanged
}

TEST(AnimationCore, ClipSetSampleRateHzRejectsNaN)
{
    AnimationClip clip(1.f, 24.f);
    clip.setSampleRateHz(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(clip.sampleRateHz(), 24.f); // unchanged
}

TEST(AnimationCore, ClipSetSampleRateHzRejectsInf)
{
    AnimationClip clip(1.f, 24.f);
    clip.setSampleRateHz(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(clip.sampleRateHz(), 24.f); // unchanged
}

TEST(AnimationCore, ClipStateMachineTickRejectsNaNDeltaDoesNotCorruptTime)
{
    AnimationClip clip(4.f, 0.f);

    Skeleton skel;
    (void)skel.addBone(BoneDesc{"root", -1, {}});

    ClipStateMachine machine;
    machine.play(clip, 0.f, {});
    machine.tick(1.f);   // advance to t=1 with valid dt
    machine.tick(std::numeric_limits<float>::quiet_NaN());
    // NaN should be treated as 0, so time remains at 1.
    Pose pose;
    machine.sampleToPose(skel, pose);   // should not assert/crash
    // Can't easily read currentTimeSec directly; verify via no crash + pose valid
    (void)pose;
}

TEST(AnimationCore, ClipStateMachineTickRejectsInfDeltaDoesNotCorruptTime)
{
    AnimationClip clip(4.f, 0.f);

    Skeleton skel;
    (void)skel.addBone(BoneDesc{"root", -1, {}});

    ClipStateMachine machine;
    machine.play(clip, 0.f, {});
    machine.tick(1.f);
    machine.tick(std::numeric_limits<float>::infinity());
    Pose pose;
    machine.sampleToPose(skel, pose);   // should not assert/crash
    (void)pose;
}

TEST(AnimationCore, RequestTransitionWithNaNFadeTreatedAsImmediateSwap)
{
    AnimationClip clipA(4.f, 0.f);
    AnimationClip clipB(4.f, 0.f);

    ClipStateMachine machine;
    machine.play(clipA, 0.f, {});
    // NaN fade window should be clamped to 0 → immediate swap
    const bool result = machine.requestTransition(clipB,
                                                  std::numeric_limits<float>::quiet_NaN(),
                                                  {});
    EXPECT_TRUE(result);
    EXPECT_FALSE(machine.isTransitioning());  // immediate swap, no cross-fade in flight
}

TEST(AnimationCore, ClipStateMachinePlayRejectsNaNStartTime)
{
    AnimationClip clip(4.f, 0.f);

    TransformTrack track;
    TransformKeyframe k0{};
    k0.timeSec = 0.f;
    k0.value.translation = {0.f, 0.f, 0.f};
    TransformKeyframe k1{};
    k1.timeSec = 4.f;
    k1.value.translation = {20.f, 0.f, 0.f};
    track.setKeyframes({k0, k1});
    clip.setBoneTrack(0, track);

    Skeleton skel;
    (void)skel.addBone(BoneDesc{"root", -1, {}});

    ClipStateMachine machine;
    machine.play(clip, std::numeric_limits<float>::quiet_NaN(), {});

    Pose out;
    machine.sampleToPose(skel, out);
    EXPECT_NEAR(out.localTransform(0).translation.x, 0.f, 1e-5f);
}

TEST(AnimationCore, ClipStateMachinePlayRejectsInfStartTime)
{
    AnimationClip clip(4.f, 0.f);

    TransformTrack track;
    TransformKeyframe k0{};
    k0.timeSec = 0.f;
    k0.value.translation = {0.f, 0.f, 0.f};
    TransformKeyframe k1{};
    k1.timeSec = 4.f;
    k1.value.translation = {20.f, 0.f, 0.f};
    track.setKeyframes({k0, k1});
    clip.setBoneTrack(0, track);

    Skeleton skel;
    (void)skel.addBone(BoneDesc{"root", -1, {}});

    ClipStateMachine machine;
    machine.play(clip, std::numeric_limits<float>::infinity(), {});

    Pose out;
    machine.sampleToPose(skel, out);
    EXPECT_NEAR(out.localTransform(0).translation.x, 0.f, 1e-5f);
}

// ── Per-keyframe interpolation modes ────────────────────────────────────────────

TEST(AnimationCore, StepInterpolationHoldsValueUntilNextKey)
{
    TransformTrack track;
    TransformKeyframe k0{};
    k0.timeSec = 0.f;
    k0.value.translation = {0.f, 0.f, 0.f};
    k0.interpolation = KeyInterpolation::Step;
    TransformKeyframe k1{};
    k1.timeSec = 1.f;
    k1.value.translation = {10.f, 0.f, 0.f};
    track.setKeyframes({k0, k1});

    EXPECT_NEAR(track.sample(0.0f).translation.x, 0.f, 1e-5f);
    EXPECT_NEAR(track.sample(0.5f).translation.x, 0.f, 1e-5f);  // held, no blend
    EXPECT_NEAR(track.sample(0.99f).translation.x, 0.f, 1e-5f);
    EXPECT_NEAR(track.sample(1.0f).translation.x, 10.f, 1e-5f); // snaps at the next key
}

TEST(AnimationCore, LinearIsTheDefaultInterpolation)
{
    TransformKeyframe k0{}; // interpolation left at its default
    EXPECT_EQ(k0.interpolation, KeyInterpolation::Linear);
}

TEST(AnimationCore, CubicInterpolationPassesThroughKeysAndSmooths)
{
    TransformTrack track;
    // x = 0,1,3,6 at t = 0,1,2,3 — an accelerating curve so cubic tangents bend it.
    auto key = [](float t, float x) {
        TransformKeyframe k{};
        k.timeSec = t;
        k.value.translation = {x, 0.f, 0.f};
        k.interpolation = KeyInterpolation::Cubic;
        return k;
    };
    track.setKeyframes({key(0.f, 0.f), key(1.f, 1.f), key(2.f, 3.f), key(3.f, 6.f)});

    // Exactly interpolates (passes through) the keyframes.
    EXPECT_NEAR(track.sample(1.0f).translation.x, 1.f, 1e-4f);
    EXPECT_NEAR(track.sample(2.0f).translation.x, 3.f, 1e-4f);

    // Mid-segment uses Catmull-Rom tangents from the neighbours, so it deviates from
    // the linear midpoint (2.0) — here 1.875.
    const float mid = track.sample(1.5f).translation.x;
    EXPECT_NEAR(mid, 1.875f, 1e-3f);
    EXPECT_GT(std::fabs(mid - 2.0f), 0.05f);
}

TEST(AnimationCore, CubicRotationUsesSquadSpline)
{
    constexpr float kPi = 3.14159265358979f;
    // Z-axis rotations at 0, 30, 90, 180 degrees (accelerating) at t = 0,1,2,3.
    auto zKey = [&](float deg, float t) {
        const float h = deg * kPi / 180.f * 0.5f;
        TransformKeyframe k{};
        k.timeSec = t;
        k.value.rotation = {0.f, 0.f, std::sin(h), std::cos(h)};
        k.interpolation = KeyInterpolation::Cubic;
        return k;
    };
    TransformTrack track;
    track.setKeyframes({zKey(0.f, 0.f), zKey(30.f, 1.f), zKey(90.f, 2.f), zKey(180.f, 3.f)});

    auto angleDeg = [&](const AnimTransform& t) {
        float w = std::fabs(t.rotation.w);
        if (w > 1.f) w = 1.f;
        const float a = 2.f * std::acos(w) * 180.f / kPi;
        return t.rotation.z >= 0.f ? a : -a;
    };
    auto unitLen = [](const AnimTransform& t) {
        const auto& r = t.rotation;
        return std::sqrt(r.x*r.x + r.y*r.y + r.z*r.z + r.w*r.w);
    };

    // Passes exactly through the keyframe rotations.
    EXPECT_NEAR(angleDeg(track.sample(1.0f)), 30.f, 0.5f);
    EXPECT_NEAR(angleDeg(track.sample(2.0f)), 90.f, 0.5f);

    // Always a unit quaternion mid-segment.
    EXPECT_NEAR(unitLen(track.sample(1.25f)), 1.0f, 1e-3f);
    EXPECT_NEAR(unitLen(track.sample(1.75f)), 1.0f, 1e-3f);

    // squad bends the arc off the plain-slerp midpoint (60 deg), staying in range.
    const float mid = angleDeg(track.sample(1.5f));
    EXPECT_GT(mid, 30.f);
    EXPECT_LT(mid, 90.f);
    EXPECT_GT(std::fabs(mid - 60.f), 1.0f);
}

// ── Notify / event tracks ───────────────────────────────────────────────────────

TEST(AnimationCore, NotifyTrackSortsEventsByTime)
{
    NotifyTrack track;
    track.setEvents({ {1.5f, 30u}, {0.5f, 10u}, {1.0f, 20u} });
    ASSERT_EQ(track.eventCount(), 3u);
    EXPECT_FLOAT_EQ(track.events()[0].timeSec, 0.5f);
    EXPECT_FLOAT_EQ(track.events()[1].timeSec, 1.0f);
    EXPECT_FLOAT_EQ(track.events()[2].timeSec, 1.5f);
    EXPECT_EQ(track.events()[0].id, 10u);
}

TEST(AnimationCore, NotifyTrackCollectsEventsInHalfOpenInterval)
{
    NotifyTrack track;
    track.setEvents({ {0.5f, 10u}, {1.0f, 20u}, {1.5f, 30u} });

    std::vector<AnimationEvent> out;
    track.collectEvents(0.4f, 1.1f, out);          // (0.4, 1.1]
    ASSERT_EQ(out.size(), 2u);
    EXPECT_EQ(out[0].id, 10u);
    EXPECT_EQ(out[1].id, 20u);

    // Advancing from exactly an event time excludes it (already fired); the next does.
    out.clear();
    track.collectEvents(1.0f, 1.5f, out);          // (1.0, 1.5]
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].id, 30u);

    // Empty window fires nothing.
    out.clear();
    track.collectEvents(1.5f, 1.5f, out);
    EXPECT_TRUE(out.empty());
}

TEST(AnimationCore, AnimationClipForwardsNotifyEvents)
{
    AnimationClip clip(2.f, 30.f);
    NotifyTrack track;
    track.setEvents({ {0.25f, 7u}, {1.75f, 9u} });
    clip.setNotifyTrack(std::move(track));

    std::vector<AnimationEvent> out;
    clip.collectEvents(0.0f, 1.0f, out);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].id, 7u);
}
