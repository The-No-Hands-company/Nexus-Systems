#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Animation — Skeleton Retargeting (Month 7)
//
//  Provides name-based bone mapping between skeletons and pose retargeting
//  so that an AnimationClip authored for one skeleton can drive another.
//
//  Design constraints:
//  - Pure data contract; no GPU or render objects.
//  - Name-based mapping only for v0 (no T-pose normalization).
//  - Unmapped bones fall back to the target skeleton's bind-local transform.
//  - Deterministic and headless-safe; all paths are noexcept.
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/animation/AnimationCore.h>

#include <cstdint>
#include <string>
#include <unordered_map>

namespace nexus::animation {

// ─────────────────────────────────────────────────────────────────────────────
//  SkeletonMapping
//
//  Maps source skeleton bone names to target skeleton bone indices.
//  Build automatically via buildByName() or construct manually with mapBone().
// ─────────────────────────────────────────────────────────────────────────────
class SkeletonMapping {
public:
    // Builds a mapping by matching bone names exactly (case-sensitive).
    // Source bones whose names are not present in the target are left unmapped.
    [[nodiscard]] static SkeletonMapping buildByName(const Skeleton& source,
                                                      const Skeleton& target) noexcept;

    // Maps sourceBoneName → the bone in target that has targetBoneName.
    // Returns false if targetBoneName is not found in target.
    [[nodiscard]] bool mapBone(const std::string& sourceBoneName,
                               const std::string& targetBoneName,
                               const Skeleton&    target) noexcept;

    // Returns the target bone index for the given source bone index, or
    // Skeleton::kInvalidBone if the source bone has no mapping.
    [[nodiscard]] int32_t targetBoneFor(size_t sourceBoneIndex) const noexcept;

    // Returns the target bone index for the given source bone name, or
    // Skeleton::kInvalidBone if the source bone has no mapping.
    [[nodiscard]] int32_t targetBoneForSourceName(const std::string& sourceBoneName) const noexcept;

    // Returns the number of explicit mappings.
    [[nodiscard]] size_t mappingCount() const noexcept { return m_map.size(); }

    void clear() noexcept { m_map.clear(); m_nameMap.clear(); }

private:
    // sourceBoneIndex → targetBoneIndex
    std::unordered_map<size_t, int32_t> m_map;
    // sourceBoneName  → targetBoneIndex  (used for manual mapBone lookups)
    std::unordered_map<std::string, int32_t> m_nameMap;
};

// ─────────────────────────────────────────────────────────────────────────────
//  SkeletonRetargeter
//
//  Applies a SkeletonMapping to transfer local transforms from a source Pose
//  to a target Pose.  The source Pose must have been evaluated on sourceSkeleton
//  (computeModelMatrices is not required — local transforms are sufficient).
// ─────────────────────────────────────────────────────────────────────────────
class SkeletonRetargeter {
public:
    // Retargets sourcePose (from sourceSkeleton) to outTargetPose (for targetSkeleton).
    //
    // For each bone in targetSkeleton:
    //   - If a mapping exists from a source bone → this target bone index,
    //     copy the source bone's local transform.
    //   - Otherwise use the targetSkeleton bind-local transform (identity
    //     animation for unmapped bones).
    //
    // outTargetPose is resized to targetSkeleton.boneCount().
    static void retarget(const Pose&            sourcePose,
                         const Skeleton&        sourceSkeleton,
                         const SkeletonMapping& mapping,
                         const Skeleton&        targetSkeleton,
                         Pose&                  outTargetPose) noexcept;
};

} // namespace nexus::animation
