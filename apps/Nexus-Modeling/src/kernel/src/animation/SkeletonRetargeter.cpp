#include <nexus/animation/SkeletonRetargeter.h>

namespace nexus::animation {

// ─────────────────────────────────────────────────────────────────────────────
//  SkeletonMapping
// ─────────────────────────────────────────────────────────────────────────────

SkeletonMapping SkeletonMapping::buildByName(const Skeleton& source,
                                              const Skeleton& target) noexcept
{
    // Build target name → index lookup.
    std::unordered_map<std::string, int32_t> targetIndex;
    for (size_t i = 0u; i < target.boneCount(); ++i) {
        targetIndex.emplace(target.boneName(i), static_cast<int32_t>(i));
    }

    SkeletonMapping m;
    for (size_t i = 0u; i < source.boneCount(); ++i) {
        const std::string& srcName = source.boneName(i);
        auto it = targetIndex.find(srcName);
        if (it != targetIndex.end()) {
            m.m_map.emplace(i, it->second);
            m.m_nameMap.emplace(srcName, it->second);
        }
    }
    return m;
}

bool SkeletonMapping::mapBone(const std::string& sourceBoneName,
                               const std::string& targetBoneName,
                               const Skeleton&    target) noexcept
{
    // Find targetBoneName in target skeleton.
    for (size_t i = 0u; i < target.boneCount(); ++i) {
        if (target.boneName(i) == targetBoneName) {
            // Find source index via the name map (if it was previously established),
            // or store by name only — we don't have a source skeleton here.
            // Record the name → targetIndex so targetBoneFor can still resolve it
            // when the source bone index is known.
            m_nameMap[sourceBoneName] = static_cast<int32_t>(i);
            return true;
        }
    }
    return false;
}

int32_t SkeletonMapping::targetBoneFor(size_t sourceBoneIndex) const noexcept
{
    auto it = m_map.find(sourceBoneIndex);
    if (it == m_map.end()) { return Skeleton::kInvalidBone; }
    return it->second;
}

int32_t SkeletonMapping::targetBoneForSourceName(const std::string& sourceBoneName) const noexcept
{
    auto it = m_nameMap.find(sourceBoneName);
    if (it == m_nameMap.end()) {
        return Skeleton::kInvalidBone;
    }
    return it->second;
}

// ─────────────────────────────────────────────────────────────────────────────
//  SkeletonRetargeter
// ─────────────────────────────────────────────────────────────────────────────

void SkeletonRetargeter::retarget(const Pose&            sourcePose,
                                   const Skeleton&        sourceSkeleton,
                                   const SkeletonMapping& mapping,
                                   const Skeleton&        targetSkeleton,
                                   Pose&                  outTargetPose) noexcept
{
    const size_t targetBoneCount = targetSkeleton.boneCount();
    outTargetPose.resize(targetBoneCount);

    // Prime every target bone with its bind-local (neutral pose).
    for (size_t ti = 0u; ti < targetBoneCount; ++ti) {
        outTargetPose.setLocalTransform(ti, targetSkeleton.bindLocal(ti));
    }

    // Overwrite mapped bones with the source local transform.
    for (size_t si = 0u; si < sourceSkeleton.boneCount(); ++si) {
        int32_t ti = mapping.targetBoneFor(si);
        if (ti < 0) {
            ti = mapping.targetBoneForSourceName(sourceSkeleton.boneName(si));
        }
        if (ti < 0 || static_cast<size_t>(ti) >= targetBoneCount) { continue; }
        outTargetPose.setLocalTransform(static_cast<size_t>(ti),
                                        sourcePose.localTransform(si));
    }
}

} // namespace nexus::animation
