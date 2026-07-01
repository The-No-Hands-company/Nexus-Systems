#pragma once
// ─── Nexus Animation ── VideoEditor ───────────────────────────────────────

#include <cstdint>
#include <string>
#include <vector>

namespace nexus::animation {

struct VideoTrack {
    uint32_t id = 0;
    std::string name;
};

struct VideoClip {
    uint32_t id = 0;
    uint32_t trackId = 0;
    float timelineStart = 0.0f;
    float timelineEnd   = 0.0f;
    float sourceStart   = 0.0f;
    float sourceEnd     = 0.0f;
};

struct Transition {
    uint32_t fromClip = 0;
    uint32_t toClip   = 0;
    float duration    = 0.0f;
};

class VideoEditor {
public:
    [[nodiscard]] static uint32_t addTrack(const std::string& name);

    [[nodiscard]] static uint32_t addClip(const VideoClip& c);

    static void addTransition(const Transition& t);

    static void trimClip(uint32_t cid, float sourceStart, float sourceEnd);

    static void moveClip(uint32_t cid, float timelineStart);

    static void splitClip(uint32_t cid, float splitTime);

    [[nodiscard]] static float getTotalDuration();

    [[nodiscard]] static std::vector<VideoClip> getClipsAtTime(float time);

    [[nodiscard]] static std::vector<VideoClip> getClipsInRange(float start, float end);

    [[nodiscard]] static float getTransitionAlpha(uint32_t fromClip, uint32_t toClip, float time);

    static void rippleDelete(uint32_t cid);

    static void snapToClip(uint32_t movingClipId, float& proposedTime);
};

} // namespace nexus::animation
