#include <nexus/animation/AnimationSerialization.h>

#include <cstdio>
#include <cstring>

namespace nexus::animation {

namespace {

// ── Write helpers ─────────────────────────────────────────────────────────────
bool writeU32(std::FILE* fp, uint32_t v) noexcept
{
    return std::fwrite(&v, sizeof(v), 1, fp) == 1;
}

bool writeF32(std::FILE* fp, float v) noexcept
{
    return std::fwrite(&v, sizeof(v), 1, fp) == 1;
}

bool writeU8(std::FILE* fp, uint8_t v) noexcept
{
    return std::fwrite(&v, sizeof(v), 1, fp) == 1;
}

// ── Read helpers ──────────────────────────────────────────────────────────────
bool readU32(std::FILE* fp, uint32_t& out) noexcept
{
    return std::fread(&out, sizeof(out), 1, fp) == 1;
}

bool readF32(std::FILE* fp, float& out) noexcept
{
    return std::fread(&out, sizeof(out), 1, fp) == 1;
}

bool readU8(std::FILE* fp, uint8_t& out) noexcept
{
    return std::fread(&out, sizeof(out), 1, fp) == 1;
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
//  Save
// ─────────────────────────────────────────────────────────────────────────────
AnimClipIOReport AnimationClipSerializer::save(const AnimationClip& clip,
                                                const std::string& path) noexcept
{
    AnimClipIOReport report;

    if (path.empty()) {
        report.diagnostic = AnimClipDiagnostic::FileOpenFailed;
        report.messages.push_back("Path is empty");
        return report;
    }

    std::FILE* fp = std::fopen(path.c_str(), "wb");
    if (!fp) {
        report.diagnostic = AnimClipDiagnostic::FileOpenFailed;
        report.messages.push_back("Failed to open file for writing: " + path);
        return report;
    }

    auto fail = [&](const std::string& msg) -> AnimClipIOReport {
        std::fclose(fp);
        report.diagnostic = AnimClipDiagnostic::WriteError;
        report.messages.push_back(msg);
        return report;
    };

    // Collect non-empty track indices (sorted ascending for determinism).
    const size_t totalTracks = clip.trackSlotCount();
    std::vector<size_t> trackIndices;
    trackIndices.reserve(totalTracks);
    for (size_t i = 0; i < totalTracks; ++i) {
        if (clip.hasBoneTrack(i)) {
            trackIndices.push_back(i);
        }
    }

    const uint32_t storedTrackCount = static_cast<uint32_t>(trackIndices.size());

    // Header
    if (!writeU32(fp, kAnimClipMagic))                       { return fail("Write error: magic"); }
    if (!writeU32(fp, kAnimClipVersionCurrent))              { return fail("Write error: version"); }
    if (!writeF32(fp, clip.durationSec()))                   { return fail("Write error: durationSec"); }
    if (!writeF32(fp, clip.sampleRateHz()))                  { return fail("Write error: sampleRateHz"); }
    if (!writeU8(fp, static_cast<uint8_t>(clip.wrapMode()))) { return fail("Write error: wrapMode"); }
    if (!writeU32(fp, storedTrackCount))                     { return fail("Write error: trackCount"); }

    // Tracks — write exact authored keyframes for true round-trip fidelity.
    for (const size_t boneIdx : trackIndices) {
        const uint32_t boneIndexU32 = static_cast<uint32_t>(boneIdx);
        if (!writeU32(fp, boneIndexU32)) { return fail("Write error: boneIndex"); }

        const TransformTrack* track = clip.boneTrack(boneIdx);
        if (!track) {
            return fail("Write error: missing track during indexed save traversal");
        }

        const uint32_t keyCount = static_cast<uint32_t>(track->keyframeCount());
        if (!writeU32(fp, keyCount)) { return fail("Write error: keyframeCount"); }

        for (uint32_t keyIndex = 0; keyIndex < keyCount; ++keyIndex) {
            const TransformKeyframe* key = track->keyframe(static_cast<size_t>(keyIndex));
            if (!key) {
                return fail("Write error: null keyframe handle");
            }
            const Transform& xf = key->value;

            if (!writeF32(fp, key->timeSec)) { return fail("Write error: keyframe timeSec"); }
            if (!writeF32(fp, xf.translation.x)) { return fail("Write error: tx"); }
            if (!writeF32(fp, xf.translation.y)) { return fail("Write error: ty"); }
            if (!writeF32(fp, xf.translation.z)) { return fail("Write error: tz"); }
            if (!writeF32(fp, xf.rotation.x))    { return fail("Write error: rx"); }
            if (!writeF32(fp, xf.rotation.y))    { return fail("Write error: ry"); }
            if (!writeF32(fp, xf.rotation.z))    { return fail("Write error: rz"); }
            if (!writeF32(fp, xf.rotation.w))    { return fail("Write error: rw"); }
            if (!writeF32(fp, xf.scale.x))       { return fail("Write error: sx"); }
            if (!writeF32(fp, xf.scale.y))       { return fail("Write error: sy"); }
            if (!writeF32(fp, xf.scale.z))       { return fail("Write error: sz"); }
        }
    }

    std::fclose(fp);

    report.valid      = true;
    report.version    = kAnimClipVersionCurrent;
    report.trackCount = storedTrackCount;
    return report;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Load
// ─────────────────────────────────────────────────────────────────────────────
AnimClipIOReport AnimationClipSerializer::load(const std::string& path,
                                                AnimationClip& outClip) noexcept
{
    AnimClipIOReport report;

    if (path.empty()) {
        report.diagnostic = AnimClipDiagnostic::FileOpenFailed;
        report.messages.push_back("Path is empty");
        return report;
    }

    std::FILE* fp = std::fopen(path.c_str(), "rb");
    if (!fp) {
        report.diagnostic = AnimClipDiagnostic::FileOpenFailed;
        report.messages.push_back("Failed to open file for reading: " + path);
        return report;
    }

    auto fail = [&](AnimClipDiagnostic diag, const std::string& msg) -> AnimClipIOReport {
        std::fclose(fp);
        report.diagnostic = diag;
        report.messages.push_back(msg);
        return report;
    };

    uint32_t magic = 0u, version = 0u;
    if (!readU32(fp, magic))   { return fail(AnimClipDiagnostic::ReadError, "Read error: magic"); }
    if (magic != kAnimClipMagic) {
        return fail(AnimClipDiagnostic::InvalidMagic, "Invalid magic bytes");
    }
    if (!readU32(fp, version)) { return fail(AnimClipDiagnostic::ReadError, "Read error: version"); }
    if (version > kAnimClipVersionCurrent) {
        return fail(AnimClipDiagnostic::UnsupportedVersion,
                    "Unsupported clip version: " + std::to_string(version));
    }

    float    durationSec = 0.f, sampleRateHz = 0.f;
    uint8_t  wrapModeRaw = 0u;
    uint32_t trackCount  = 0u;

    if (!readF32(fp, durationSec))   { return fail(AnimClipDiagnostic::ReadError, "Read error: durationSec"); }
    if (!readF32(fp, sampleRateHz))  { return fail(AnimClipDiagnostic::ReadError, "Read error: sampleRateHz"); }
    if (!readU8(fp,  wrapModeRaw))   { return fail(AnimClipDiagnostic::ReadError, "Read error: wrapMode"); }
    if (!readU32(fp, trackCount))    { return fail(AnimClipDiagnostic::ReadError, "Read error: trackCount"); }

    if (wrapModeRaw > 1u) {
        return fail(AnimClipDiagnostic::InvalidData,
                    "Invalid wrapMode value: " + std::to_string(wrapModeRaw));
    }

    AnimationClip loaded(durationSec, sampleRateHz);
    loaded.setWrapMode(wrapModeRaw == 0u ? ClipWrapMode::Clamp : ClipWrapMode::Loop);

    for (uint32_t t = 0u; t < trackCount; ++t) {
        uint32_t boneIndex = 0u, keyframeCount = 0u;
        if (!readU32(fp, boneIndex))    { return fail(AnimClipDiagnostic::ReadError, "Read error: boneIndex"); }
        if (!readU32(fp, keyframeCount)) { return fail(AnimClipDiagnostic::ReadError, "Read error: keyframeCount"); }

        std::vector<TransformKeyframe> keys;
        keys.reserve(keyframeCount);

        for (uint32_t k = 0u; k < keyframeCount; ++k) {
            TransformKeyframe kf;
            if (!readF32(fp, kf.timeSec))            { return fail(AnimClipDiagnostic::ReadError, "Read error: keyframe timeSec"); }
            if (!readF32(fp, kf.value.translation.x)) { return fail(AnimClipDiagnostic::ReadError, "Read error: tx"); }
            if (!readF32(fp, kf.value.translation.y)) { return fail(AnimClipDiagnostic::ReadError, "Read error: ty"); }
            if (!readF32(fp, kf.value.translation.z)) { return fail(AnimClipDiagnostic::ReadError, "Read error: tz"); }
            if (!readF32(fp, kf.value.rotation.x))    { return fail(AnimClipDiagnostic::ReadError, "Read error: rx"); }
            if (!readF32(fp, kf.value.rotation.y))    { return fail(AnimClipDiagnostic::ReadError, "Read error: ry"); }
            if (!readF32(fp, kf.value.rotation.z))    { return fail(AnimClipDiagnostic::ReadError, "Read error: rz"); }
            if (!readF32(fp, kf.value.rotation.w))    { return fail(AnimClipDiagnostic::ReadError, "Read error: rw"); }
            if (!readF32(fp, kf.value.scale.x))       { return fail(AnimClipDiagnostic::ReadError, "Read error: sx"); }
            if (!readF32(fp, kf.value.scale.y))       { return fail(AnimClipDiagnostic::ReadError, "Read error: sy"); }
            if (!readF32(fp, kf.value.scale.z))       { return fail(AnimClipDiagnostic::ReadError, "Read error: sz"); }
            keys.push_back(kf);
        }

        TransformTrack track;
        track.setKeyframes(std::move(keys));
        loaded.setBoneTrack(static_cast<size_t>(boneIndex), std::move(track));
    }

    std::fclose(fp);

    outClip = std::move(loaded);

    report.valid      = true;
    report.version    = version;
    report.trackCount = trackCount;
    return report;
}

} // namespace nexus::animation
