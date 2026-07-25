#include <nexus/animation/VideoEditor.h>

#include <algorithm>
#include <cmath>

namespace nexus::animation {

static std::vector<VideoTrack> g_tracks;
static std::vector<VideoClip> g_clips;
static std::vector<Transition> g_trans;
static uint32_t g_tid = 1, g_cid = 1;

uint32_t VideoEditor::addTrack(const std::string& name) {
    VideoTrack t;
    t.name = name;
    t.id = g_tid++;
    g_tracks.push_back(t);
    return t.id;
}

uint32_t VideoEditor::addClip(const VideoClip& c) {
    VideoClip cl = c;
    cl.id = g_cid++;
    g_clips.push_back(cl);
    return cl.id;
}

void VideoEditor::addTransition(const Transition& t) {
    g_trans.push_back(t);
}

void VideoEditor::trimClip(uint32_t cid, float sourceStart, float sourceEnd) {
    for (auto& c : g_clips) {
        if (c.id == cid) {
            c.sourceStart = sourceStart;
            c.sourceEnd = sourceEnd;
            c.timelineEnd = c.timelineStart + (sourceEnd - sourceStart);
        }
    }
}

void VideoEditor::moveClip(uint32_t cid, float timelineStart) {
    for (auto& c : g_clips) {
        if (c.id == cid) {
            float d = c.timelineEnd - c.timelineStart;
            c.timelineStart = timelineStart;
            c.timelineEnd = timelineStart + d;
        }
    }
}

void VideoEditor::splitClip(uint32_t cid, float splitTime) {
    // Iterate by index over a FIXED bound, not a range-for: g_clips.push_back below can
    // reallocate the vector, which invalidates a range-for's cached begin/end iterators and
    // the reference `c` — undefined behaviour. Indexing re-fetches each element and the bound
    // n is captured before any insertion, so the appended clip is never re-scanned (its id is
    // new anyway). We copy the split-off fields out before the push_back, since the reference
    // into g_clips is dangling the moment it reallocates.
    const size_t n = g_clips.size();
    for (size_t i = 0; i < n; ++i) {
        VideoClip& c = g_clips[i];
        if (c.id != cid || splitTime <= c.timelineStart || splitTime >= c.timelineEnd) continue;

        VideoClip newClip = c;
        newClip.id = g_cid++;
        newClip.timelineStart = splitTime;
        newClip.sourceStart = c.sourceStart + (splitTime - c.timelineStart);
        c.timelineEnd = splitTime;
        c.sourceEnd = newClip.sourceStart;
        g_clips.push_back(newClip);  // may reallocate; c is not touched afterwards
    }
}

float VideoEditor::getTotalDuration() {
    float mx = 0;
    for (auto& c : g_clips) mx = std::max(mx, c.timelineEnd);
    return mx;
}

std::vector<VideoClip> VideoEditor::getClipsAtTime(float time) {
    std::vector<VideoClip> active;
    for (auto& c : g_clips) {
        if (time >= c.timelineStart && time < c.timelineEnd) {
            active.push_back(c);
        }
    }
    std::sort(active.begin(), active.end(), [](const VideoClip& a, const VideoClip& b) {
        return a.trackId < b.trackId;
    });
    return active;
}

std::vector<VideoClip> VideoEditor::getClipsInRange(float start, float end) {
    std::vector<VideoClip> active;
    for (auto& c : g_clips) {
        if (c.timelineEnd > start && c.timelineStart < end) {
            active.push_back(c);
        }
    }
    return active;
}

float VideoEditor::getTransitionAlpha(uint32_t fromClip, uint32_t toClip, float time) {
    for (auto& t : g_trans) {
        if (t.fromClip == fromClip && t.toClip == toClip) {
            float transStart = 0;
            for (auto& c : g_clips) {
                if (c.id == toClip) { transStart = c.timelineStart; break; }
            }
            // An instantaneous (zero- or negative-duration) transition has no ramp: it is
            // fully "to" from its start onward. Guard before dividing.
            if (t.duration <= 0.0f) return time >= transStart ? 1.0f : 0.0f;
            float transEnd = transStart + t.duration;
            if (time >= transStart && time <= transEnd) {
                return (time - transStart) / t.duration;
            }
            if (time > transEnd) return 1.0f;
            return 0.0f;
        }
    }
    return 1.0f;
}

void VideoEditor::rippleDelete(uint32_t cid) {
    float deletedDuration = 0.f;
    float deletedStart = 0.f;
    uint32_t deletedTrack = 0;
    bool found = false;

    for (auto it = g_clips.begin(); it != g_clips.end(); ) {
        if (it->id == cid) {
            deletedDuration = it->timelineEnd - it->timelineStart;
            deletedStart = it->timelineStart;
            deletedTrack = it->trackId;
            found = true;
            it = g_clips.erase(it);
        } else {
            ++it;
        }
    }
    if (!found) return;

    // Close the gap by pulling only the clips that came AFTER the deleted one on its track
    // toward it. The previous version shifted every clip on the track with timelineStart > 0,
    // including clips that started BEFORE the deleted clip — dragging them left (often to
    // negative time) and corrupting the timeline. Ripple-delete removes a span and closes the
    // hole; clips ahead of the hole must stay put.
    for (auto& c : g_clips) {
        if (c.trackId == deletedTrack && c.timelineStart >= deletedStart) {
            c.timelineStart -= deletedDuration;
            c.timelineEnd -= deletedDuration;
        }
    }
}

void VideoEditor::snapToClip(uint32_t movingClipId, float& proposedTime) {
    const float snapThreshold = 0.5f;
    float bestDist = snapThreshold;
    float snapTarget = proposedTime;

    for (auto& c : g_clips) {
        if (c.id == movingClipId) continue;

        float dist = std::abs(proposedTime - c.timelineStart);
        if (dist < bestDist) { bestDist = dist; snapTarget = c.timelineStart; }

        dist = std::abs(proposedTime - c.timelineEnd);
        if (dist < bestDist) { bestDist = dist; snapTarget = c.timelineEnd; }
    }

    if (bestDist < snapThreshold) proposedTime = snapTarget;
}

} // namespace nexus::animation
