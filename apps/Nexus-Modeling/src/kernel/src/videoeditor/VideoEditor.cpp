#include "nexus/animation/videoeditor2/VideoEditor.h"
#include <algorithm>
#include <cmath>

namespace nexus::videoeditor2 {
static std::vector<VideoTrack> g_tracks;
static std::vector<VideoClip> g_clips;
static std::vector<Transition> g_trans;
static uint32_t g_tid=1, g_cid=1;

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

void VideoEditor::trimClip(uint32_t cid, float ns, float ne) {
    for (auto& c : g_clips) {
        if (c.id == cid) {
            c.sourceStart = ns;
            c.sourceEnd = ne;
            c.timelineEnd = c.timelineStart + (ne - ns);
        }
    }
}

void VideoEditor::moveClip(uint32_t cid, float nts) {
    for (auto& c : g_clips) {
        if (c.id == cid) {
            float d = c.timelineEnd - c.timelineStart;
            c.timelineStart = nts;
            c.timelineEnd = nts + d;
        }
    }
}

void VideoEditor::splitClip(uint32_t cid, float st) {
    for (auto& c : g_clips) {
        if (c.id == cid && st > c.timelineStart && st < c.timelineEnd) {
            VideoClip newClip = c;
            newClip.id = g_cid++;
            newClip.timelineStart = st;
            newClip.sourceStart = c.sourceStart + (st - c.timelineStart);
            c.timelineEnd = st;
            c.sourceEnd = newClip.sourceStart;
            g_clips.push_back(newClip);
        }
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
    float deletedDuration = 0;
    uint32_t deletedTrack = 0;

    for (auto it = g_clips.begin(); it != g_clips.end(); ) {
        if (it->id == cid) {
            deletedDuration = it->timelineEnd - it->timelineStart;
            deletedTrack = it->trackId;
            it = g_clips.erase(it);
        } else {
            ++it;
        }
    }

    for (auto& c : g_clips) {
        if (c.trackId == deletedTrack && c.timelineStart > 0) {
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

}
