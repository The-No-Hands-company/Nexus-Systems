#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Animation — Clip and Skeleton Serialization
//
//  Provides save/load for AnimationClip so clips can be persisted alongside
//  SceneAssets in the Month 6 asset pipeline.
//
//  Binary format (v1):
//    [Header]
//      uint32  magic        = kAnimClipMagic ("NXAC")
//      uint32  version      = kAnimClipVersionCurrent
//      float   durationSec
//      float   sampleRateHz
//      uint8   wrapMode     (0 = Clamp, 1 = Loop)
//      uint32  trackCount   (number of per-bone track entries stored)
//    [Per track entry, trackCount times]
//      uint32  boneIndex
//      uint32  keyframeCount
//      [Per keyframe, keyframeCount times]
//        float  timeSec
//        float  tx, ty, tz      (translation)
//        float  rx, ry, rz, rw  (rotation quaternion, xyzw)
//        float  sx, sy, sz      (scale)
//
//  Design constraints:
//  - Pure data round-trip; no GPU objects.
//  - Deterministic: saved bone-track order is boneIndex ascending.
//  - Headless-safe; all paths are noexcept.
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/animation/AnimationCore.h>

#include <cstdint>
#include <string>
#include <vector>

namespace nexus::animation {

// ─────────────────────────────────────────────────────────────────────────────
//  Version constants
// ─────────────────────────────────────────────────────────────────────────────
inline constexpr uint32_t kAnimClipMagic          = 0x4E584143u;  // "NXAC"
inline constexpr uint32_t kAnimClipVersionCurrent  = 1u;

// ─────────────────────────────────────────────────────────────────────────────
//  I/O report
// ─────────────────────────────────────────────────────────────────────────────
enum class AnimClipDiagnostic : uint32_t {
    Success            = 0,
    FileOpenFailed     = 1u << 0,
    InvalidMagic       = 1u << 1,
    UnsupportedVersion = 1u << 2,
    WriteError         = 1u << 3,
    ReadError          = 1u << 4,
    InvalidData        = 1u << 5,
};

struct AnimClipIOReport {
    AnimClipDiagnostic       diagnostic  = AnimClipDiagnostic::Success;
    bool                     valid       = false;
    uint32_t                 version     = 0u;
    uint32_t                 trackCount  = 0u;
    std::vector<std::string> messages;
};

// ─────────────────────────────────────────────────────────────────────────────
//  AnimationClipSerializer
// ─────────────────────────────────────────────────────────────────────────────
class AnimationClipSerializer {
public:
    // Serialises clip to a binary file at 'path'. Always overwrites.
    [[nodiscard]] static AnimClipIOReport save(const AnimationClip& clip,
                                               const std::string& path) noexcept;

    // Deserialises a binary file into outClip (replaces existing content).
    [[nodiscard]] static AnimClipIOReport load(const std::string& path,
                                               AnimationClip& outClip) noexcept;
};

} // namespace nexus::animation
