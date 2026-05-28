#include <nexus/animation/AnimationCore.h>
#include <nexus/animation/AnimationSerialization.h>
#include <nexus/animation/SkeletonRetargeter.h>

#include <gtest/gtest.h>

#include <cstdio>
#include <filesystem>
#include <string>

namespace nexus::animation::testing {

namespace fs = std::filesystem;

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────────────────────────────────────
static std::string tmpPath(const std::string& name)
{
    return (fs::temp_directory_path() / ("nexus_animtest_" + name)).string();
}

// Build a one-bone skeleton.
static Skeleton makeSingleBoneSkeleton()
{
    Skeleton s;
    BoneDesc root;
    root.name        = "root";
    root.parentIndex = -1;
    [[maybe_unused]] auto idx = s.addBone(std::move(root));
    return s;
}

// Build a clip with one bone track: translation from (0,0,0) to (1,0,0).
static AnimationClip makeSimpleClip()
{
    AnimationClip clip(1.f, 30.f);
    clip.setWrapMode(ClipWrapMode::Clamp);

    TransformKeyframe k0;
    k0.timeSec = 0.f;
    k0.value.translation = {0.f, 0.f, 0.f};
    k0.value.rotation    = {0.f, 0.f, 0.f, 1.f};
    k0.value.scale       = {1.f, 1.f, 1.f};

    TransformKeyframe k1;
    k1.timeSec = 1.f;
    k1.value.translation = {1.f, 0.f, 0.f};
    k1.value.rotation    = {0.f, 0.f, 0.f, 1.f};
    k1.value.scale       = {1.f, 1.f, 1.f};

    TransformTrack track;
    track.setKeyframes({k0, k1});
    clip.setBoneTrack(0, std::move(track));
    return clip;
}

// ─────────────────────────────────────────────────────────────────────────────
//  AnimationClipSerializer — save / load
// ─────────────────────────────────────────────────────────────────────────────

TEST(AnimationSerialization, SaveToEmptyPathFails)
{
    AnimationClip clip = makeSimpleClip();
    const auto rep = AnimationClipSerializer::save(clip, "");
    EXPECT_FALSE(rep.valid);
}

TEST(AnimationSerialization, LoadFromEmptyPathFails)
{
    AnimationClip clip;
    const auto rep = AnimationClipSerializer::load("", clip);
    EXPECT_FALSE(rep.valid);
}

TEST(AnimationSerialization, LoadFromMissingFileFails)
{
    AnimationClip clip;
    const auto rep = AnimationClipSerializer::load("/nonexistent/clip.nxac", clip);
    EXPECT_FALSE(rep.valid);
}

TEST(AnimationSerialization, RoundTripPreservesMetadata)
{
    const std::string path = tmpPath("clip_meta.nxac");

    AnimationClip saved = makeSimpleClip();
    const auto saveRep = AnimationClipSerializer::save(saved, path);
    ASSERT_TRUE(saveRep.valid) << (saveRep.messages.empty() ? "" : saveRep.messages.front());

    AnimationClip loaded;
    const auto loadRep = AnimationClipSerializer::load(path, loaded);
    ASSERT_TRUE(loadRep.valid) << (loadRep.messages.empty() ? "" : loadRep.messages.front());

    EXPECT_FLOAT_EQ(loaded.durationSec(),  saved.durationSec());
    EXPECT_FLOAT_EQ(loaded.sampleRateHz(), saved.sampleRateHz());
    EXPECT_EQ(loaded.wrapMode(),           saved.wrapMode());

    std::remove(path.c_str());
}

TEST(AnimationSerialization, RoundTripPreservesTrackCount)
{
    const std::string path = tmpPath("clip_tracks.nxac");

    AnimationClip saved = makeSimpleClip();
    ASSERT_TRUE(AnimationClipSerializer::save(saved, path).valid);

    AnimationClip loaded;
    ASSERT_TRUE(AnimationClipSerializer::load(path, loaded).valid);

    EXPECT_EQ(loaded.trackCount(), saved.trackCount());
    EXPECT_TRUE(loaded.hasBoneTrack(0));

    std::remove(path.c_str());
}

TEST(AnimationSerialization, RoundTripPreservesLoopWrapMode)
{
    const std::string path = tmpPath("clip_loop.nxac");

    AnimationClip saved(2.f, 24.f);
    saved.setWrapMode(ClipWrapMode::Loop);

    TransformTrack track;
    TransformKeyframe k;
    k.timeSec              = 0.f;
    k.value.translation    = {0.f, 1.f, 0.f};
    k.value.rotation       = {0.f, 0.f, 0.f, 1.f};
    k.value.scale          = {1.f, 1.f, 1.f};
    track.setKeyframes({k});
    saved.setBoneTrack(0, std::move(track));

    ASSERT_TRUE(AnimationClipSerializer::save(saved, path).valid);

    AnimationClip loaded;
    ASSERT_TRUE(AnimationClipSerializer::load(path, loaded).valid);

    EXPECT_EQ(loaded.wrapMode(), ClipWrapMode::Loop);

    std::remove(path.c_str());
}

TEST(AnimationSerialization, RoundTripSampledTranslationWithinTolerance)
{
    const std::string path = tmpPath("clip_sample.nxac");

    const Skeleton skel = makeSingleBoneSkeleton();
    AnimationClip  saved = makeSimpleClip();
    ASSERT_TRUE(AnimationClipSerializer::save(saved, path).valid);

    AnimationClip loaded;
    ASSERT_TRUE(AnimationClipSerializer::load(path, loaded).valid);

    // Sample at t=0 and t=1; translation should match within floating-point tolerance.
    Pose pSaved, pLoaded;

    saved.sampleToPose(0.f, skel, pSaved);
    loaded.sampleToPose(0.f, skel, pLoaded);
    EXPECT_NEAR(pLoaded.localTransform(0).translation.x,
                pSaved.localTransform(0).translation.x, 1e-4f);

    saved.sampleToPose(1.f, skel, pSaved);
    loaded.sampleToPose(1.f, skel, pLoaded);
    EXPECT_NEAR(pLoaded.localTransform(0).translation.x,
                pSaved.localTransform(0).translation.x, 1e-4f);

    std::remove(path.c_str());
}

TEST(AnimationSerialization, RoundTripMultipleBoneTracks)
{
    const std::string path = tmpPath("clip_multitracks.nxac");

    AnimationClip saved(1.f, 10.f);

    for (size_t i = 0u; i < 3u; ++i) {
        TransformKeyframe k;
        k.timeSec           = 0.f;
        k.value.translation = {static_cast<float>(i), 0.f, 0.f};
        k.value.rotation    = {0.f, 0.f, 0.f, 1.f};
        k.value.scale       = {1.f, 1.f, 1.f};
        TransformTrack t;
        t.setKeyframes({k});
        saved.setBoneTrack(i, std::move(t));
    }

    ASSERT_TRUE(AnimationClipSerializer::save(saved, path).valid);

    AnimationClip loaded;
    ASSERT_TRUE(AnimationClipSerializer::load(path, loaded).valid);

    EXPECT_TRUE(loaded.hasBoneTrack(0));
    EXPECT_TRUE(loaded.hasBoneTrack(1));
    EXPECT_TRUE(loaded.hasBoneTrack(2));

    std::remove(path.c_str());
}

TEST(AnimationSerialization, LoadInvalidMagicFails)
{
    const std::string path = tmpPath("clip_badmagic.nxac");

    // Write a file with wrong magic bytes.
    std::FILE* fp = std::fopen(path.c_str(), "wb");
    ASSERT_NE(fp, nullptr);
    const uint32_t badMagic = 0xDEADBEEFu;
    std::fwrite(&badMagic, sizeof(badMagic), 1, fp);
    std::fclose(fp);

    AnimationClip loaded;
    const auto rep = AnimationClipSerializer::load(path, loaded);
    EXPECT_FALSE(rep.valid);

    std::remove(path.c_str());
}

TEST(AnimationSerialization, SaveAndLoadReportVersionCurrent)
{
    const std::string path = tmpPath("clip_version.nxac");

    AnimationClip saved = makeSimpleClip();
    const auto saveRep = AnimationClipSerializer::save(saved, path);
    ASSERT_TRUE(saveRep.valid);
    EXPECT_EQ(saveRep.version, kAnimClipVersionCurrent);

    AnimationClip loaded;
    const auto loadRep = AnimationClipSerializer::load(path, loaded);
    ASSERT_TRUE(loadRep.valid);
    EXPECT_EQ(loadRep.version, kAnimClipVersionCurrent);

    std::remove(path.c_str());
}

TEST(AnimationSerialization, DeterministicSaveProducesSameBytes)
{
    const std::string path1 = tmpPath("clip_det1.nxac");
    const std::string path2 = tmpPath("clip_det2.nxac");

    AnimationClip c = makeSimpleClip();
    ASSERT_TRUE(AnimationClipSerializer::save(c, path1).valid);
    ASSERT_TRUE(AnimationClipSerializer::save(c, path2).valid);

    EXPECT_EQ(fs::file_size(path1), fs::file_size(path2));

    std::remove(path1.c_str());
    std::remove(path2.c_str());
}

TEST(AnimationSerialization, RoundTripPreservesExactNonUniformKeyframes)
{
    const std::string path = tmpPath("clip_exact_keys.nxac");

    AnimationClip saved(3.f, 60.f);
    TransformTrack track;

    TransformKeyframe k0;
    k0.timeSec = 0.13f;
    k0.value.translation = {1.5f, -2.0f, 0.25f};
    k0.value.rotation = {0.1f, 0.2f, 0.3f, 0.9f};
    k0.value.scale = {1.f, 1.1f, 0.9f};

    TransformKeyframe k1;
    k1.timeSec = 1.73f;
    k1.value.translation = {-3.0f, 4.5f, 2.25f};
    k1.value.rotation = {-0.2f, 0.7f, 0.1f, 0.67f};
    k1.value.scale = {0.8f, 0.7f, 1.3f};

    track.setKeyframes({k0, k1});
    saved.setBoneTrack(5u, std::move(track));

    ASSERT_TRUE(AnimationClipSerializer::save(saved, path).valid);

    AnimationClip loaded;
    ASSERT_TRUE(AnimationClipSerializer::load(path, loaded).valid);
    ASSERT_TRUE(loaded.hasBoneTrack(5u));

    const TransformTrack* loadedTrack = loaded.boneTrack(5u);
    ASSERT_NE(loadedTrack, nullptr);
    ASSERT_EQ(loadedTrack->keyframeCount(), 2u);

    const TransformKeyframe* loadedK0 = loadedTrack->keyframe(0u);
    const TransformKeyframe* loadedK1 = loadedTrack->keyframe(1u);
    ASSERT_NE(loadedK0, nullptr);
    ASSERT_NE(loadedK1, nullptr);

    EXPECT_FLOAT_EQ(loadedK0->timeSec, 0.13f);
    EXPECT_FLOAT_EQ(loadedK0->value.translation.x, 1.5f);
    EXPECT_FLOAT_EQ(loadedK0->value.rotation.z, 0.3f);
    EXPECT_FLOAT_EQ(loadedK0->value.scale.y, 1.1f);

    EXPECT_FLOAT_EQ(loadedK1->timeSec, 1.73f);
    EXPECT_FLOAT_EQ(loadedK1->value.translation.y, 4.5f);
    EXPECT_FLOAT_EQ(loadedK1->value.rotation.x, -0.2f);
    EXPECT_FLOAT_EQ(loadedK1->value.scale.z, 1.3f);

    std::remove(path.c_str());
}

// ─────────────────────────────────────────────────────────────────────────────
//  SkeletonRetargeter
// ─────────────────────────────────────────────────────────────────────────────

static Skeleton makeTwoBoneSkeleton(const std::string& name0, const std::string& name1)
{
    Skeleton s;
    BoneDesc b0;
    b0.name        = name0;
    b0.parentIndex = -1;
    [[maybe_unused]] auto i0 = s.addBone(std::move(b0));

    BoneDesc b1;
    b1.name        = name1;
    b1.parentIndex = 0;
    [[maybe_unused]] auto i1 = s.addBone(std::move(b1));
    return s;
}

TEST(SkeletonRetargeter, BuildByNameMapsMatchingBones)
{
    const Skeleton src = makeTwoBoneSkeleton("hips", "spine");
    const Skeleton tgt = makeTwoBoneSkeleton("hips", "spine");

    const SkeletonMapping m = SkeletonMapping::buildByName(src, tgt);
    EXPECT_EQ(m.mappingCount(), 2u);
    EXPECT_EQ(m.targetBoneFor(0), 0);
    EXPECT_EQ(m.targetBoneFor(1), 1);
}

TEST(SkeletonRetargeter, BuildByNameIgnoresUnmatchedBones)
{
    const Skeleton src = makeTwoBoneSkeleton("hips", "spine");
    const Skeleton tgt = makeTwoBoneSkeleton("pelvis", "chest"); // different names

    const SkeletonMapping m = SkeletonMapping::buildByName(src, tgt);
    EXPECT_EQ(m.mappingCount(), 0u);
    EXPECT_EQ(m.targetBoneFor(0), Skeleton::kInvalidBone);
    EXPECT_EQ(m.targetBoneFor(1), Skeleton::kInvalidBone);
}

TEST(SkeletonRetargeter, BuildByNamePartialMatch)
{
    const Skeleton src = makeTwoBoneSkeleton("hips", "spine");
    const Skeleton tgt = makeTwoBoneSkeleton("hips", "chest"); // only hips matches

    const SkeletonMapping m = SkeletonMapping::buildByName(src, tgt);
    EXPECT_EQ(m.mappingCount(), 1u);
    EXPECT_EQ(m.targetBoneFor(0), 0);  // hips maps
    EXPECT_EQ(m.targetBoneFor(1), Skeleton::kInvalidBone); // spine doesn't
}

TEST(SkeletonRetargeter, RetargetMappedBonesCopiesLocalTransform)
{
    const Skeleton src = makeTwoBoneSkeleton("hips", "spine");
    const Skeleton tgt = makeTwoBoneSkeleton("hips", "spine");

    const SkeletonMapping mapping = SkeletonMapping::buildByName(src, tgt);

    // Set up source pose with a distinctive translation on hips (bone 0).
    Pose srcPose(2u);
    Transform hip;
    hip.translation = {3.f, 0.f, 0.f};
    hip.rotation    = {0.f, 0.f, 0.f, 1.f};
    hip.scale       = {1.f, 1.f, 1.f};
    srcPose.setLocalTransform(0, hip);

    Transform spineT;
    spineT.translation = {0.f, 1.f, 0.f};
    spineT.rotation    = {0.f, 0.f, 0.f, 1.f};
    spineT.scale       = {1.f, 1.f, 1.f};
    srcPose.setLocalTransform(1, spineT);

    Pose tgtPose;
    SkeletonRetargeter::retarget(srcPose, src, mapping, tgt, tgtPose);

    EXPECT_FLOAT_EQ(tgtPose.localTransform(0).translation.x, 3.f);
    EXPECT_FLOAT_EQ(tgtPose.localTransform(1).translation.y, 1.f);
}

TEST(SkeletonRetargeter, UnmappedBonesUseBindLocal)
{
    const Skeleton src = makeTwoBoneSkeleton("hips", "spine");

    // Target has a different second bone that won't be mapped.
    Skeleton tgt;
    BoneDesc b0;
    b0.name        = "hips";
    b0.parentIndex = -1;
    [[maybe_unused]] const int32_t t0 = tgt.addBone(std::move(b0));

    BoneDesc b1;
    b1.name        = "unmapped_bone";
    b1.parentIndex = 0;
    Transform customBind;
    customBind.translation = {5.f, 0.f, 0.f};
    customBind.rotation    = {0.f, 0.f, 0.f, 1.f};
    customBind.scale       = {1.f, 1.f, 1.f};
    b1.bindLocal           = customBind;
    [[maybe_unused]] const int32_t t1 = tgt.addBone(std::move(b1));

    const SkeletonMapping mapping = SkeletonMapping::buildByName(src, tgt);
    EXPECT_EQ(mapping.mappingCount(), 1u);  // only hips

    Pose srcPose(2u);
    Pose tgtPose;
    SkeletonRetargeter::retarget(srcPose, src, mapping, tgt, tgtPose);

    // Unmapped bone 1 should use its bind-local translation.
    EXPECT_FLOAT_EQ(tgtPose.localTransform(1).translation.x, 5.f);
}

TEST(SkeletonRetargeter, RetargetResizesTargetPose)
{
    const Skeleton src = makeTwoBoneSkeleton("a", "b");
    const Skeleton tgt = makeTwoBoneSkeleton("a", "b");
    const SkeletonMapping mapping = SkeletonMapping::buildByName(src, tgt);

    Pose srcPose(2u);
    Pose tgtPose; // starts empty
    SkeletonRetargeter::retarget(srcPose, src, mapping, tgt, tgtPose);

    // Must have been resized to target bone count.
    EXPECT_NO_FATAL_FAILURE({
        const Transform& t0 = tgtPose.localTransform(0);
        (void)t0;
    });
    EXPECT_NO_FATAL_FAILURE({
        const Transform& t1 = tgtPose.localTransform(1);
        (void)t1;
    });
}

TEST(SkeletonRetargeter, EmptyMappingLeavesAllBonesAtBindLocal)
{
    const Skeleton src = makeTwoBoneSkeleton("srcA", "srcB");
    const Skeleton tgt = makeTwoBoneSkeleton("tgtA", "tgtB");

    const SkeletonMapping mapping = SkeletonMapping::buildByName(src, tgt); // no matches

    Pose srcPose(2u);
    Transform t;
    t.translation = {99.f, 0.f, 0.f};
    t.rotation    = {0.f, 0.f, 0.f, 1.f};
    t.scale       = {1.f, 1.f, 1.f};
    srcPose.setLocalTransform(0, t);
    srcPose.setLocalTransform(1, t);

    Pose tgtPose;
    SkeletonRetargeter::retarget(srcPose, src, mapping, tgt, tgtPose);

    // Both target bones should remain at bind-local (default identity transform).
    EXPECT_FLOAT_EQ(tgtPose.localTransform(0).translation.x, 0.f);
    EXPECT_FLOAT_EQ(tgtPose.localTransform(1).translation.x, 0.f);
}

TEST(SkeletonRetargeter, ManualMapBoneResolvesBySourceNameDuringRetarget)
{
    const Skeleton src = makeTwoBoneSkeleton("src_hips", "src_spine");

    Skeleton tgt;
    BoneDesc t0;
    t0.name = "tgt_pelvis";
    t0.parentIndex = -1;
    ASSERT_NE(tgt.addBone(std::move(t0)), Skeleton::kInvalidBone);

    BoneDesc t1;
    t1.name = "tgt_chest";
    t1.parentIndex = 0;
    ASSERT_NE(tgt.addBone(std::move(t1)), Skeleton::kInvalidBone);

    SkeletonMapping mapping;
    ASSERT_TRUE(mapping.mapBone("src_spine", "tgt_chest", tgt));
    EXPECT_EQ(mapping.mappingCount(), 0u);
    EXPECT_EQ(mapping.targetBoneForSourceName("src_spine"), 1);

    Pose srcPose(2u);
    Transform srcSpine;
    srcSpine.translation = {0.f, 9.f, 0.f};
    srcSpine.rotation = {0.f, 0.f, 0.f, 1.f};
    srcSpine.scale = {1.f, 1.f, 1.f};
    srcPose.setLocalTransform(1u, srcSpine);

    Pose tgtPose;
    SkeletonRetargeter::retarget(srcPose, src, mapping, tgt, tgtPose);

    EXPECT_FLOAT_EQ(tgtPose.localTransform(1u).translation.y, 9.f);
}

TEST(AnimationSerialization, RoundTripPreservesKeyframeInterpolation)
{
    const std::string path = tmpPath("clip_interp.nxac");

    AnimationClip saved(2.f, 30.f);
    TransformTrack track;

    TransformKeyframe k0; k0.timeSec = 0.f; k0.interpolation = KeyInterpolation::Step;
    TransformKeyframe k1; k1.timeSec = 1.f; k1.interpolation = KeyInterpolation::Cubic;
    TransformKeyframe k2; k2.timeSec = 2.f; k2.interpolation = KeyInterpolation::Linear;
    track.setKeyframes({k0, k1, k2});
    saved.setBoneTrack(0u, std::move(track));

    ASSERT_TRUE(AnimationClipSerializer::save(saved, path).valid);

    AnimationClip loaded;
    ASSERT_TRUE(AnimationClipSerializer::load(path, loaded).valid);
    const TransformTrack* t = loaded.boneTrack(0u);
    ASSERT_NE(t, nullptr);
    ASSERT_EQ(t->keyframeCount(), 3u);
    EXPECT_EQ(t->keyframe(0u)->interpolation, KeyInterpolation::Step);
    EXPECT_EQ(t->keyframe(1u)->interpolation, KeyInterpolation::Cubic);
    EXPECT_EQ(t->keyframe(2u)->interpolation, KeyInterpolation::Linear);
}

TEST(AnimationSerialization, RoundTripPreservesNotifyEvents)
{
    const std::string path = tmpPath("clip_notifies.nxac");

    AnimationClip saved(2.f, 30.f);
    // A bone track so the file has tracks too, exercising the notify section after them.
    TransformTrack track;
    TransformKeyframe k0; k0.timeSec = 0.f;
    track.setKeyframes({k0});
    saved.setBoneTrack(0u, std::move(track));

    NotifyTrack notifies;
    notifies.setEvents({ {1.5f, 99u}, {0.5f, 11u} }); // unsorted on input
    saved.setNotifyTrack(std::move(notifies));

    ASSERT_TRUE(AnimationClipSerializer::save(saved, path).valid);

    AnimationClip loaded;
    ASSERT_TRUE(AnimationClipSerializer::load(path, loaded).valid);
    const auto& evs = loaded.notifyTrack().events();
    ASSERT_EQ(evs.size(), 2u);
    EXPECT_FLOAT_EQ(evs[0].timeSec, 0.5f); // stored sorted by time
    EXPECT_EQ(evs[0].id, 11u);
    EXPECT_FLOAT_EQ(evs[1].timeSec, 1.5f);
    EXPECT_EQ(evs[1].id, 99u);
}

} // namespace nexus::animation::testing
