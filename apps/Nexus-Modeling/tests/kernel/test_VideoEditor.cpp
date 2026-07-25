#include <gtest/gtest.h>

#include <nexus/animation/VideoEditor.h>

#include <algorithm>
#include <cmath>
#include <vector>

// VideoEditor is a global-singleton NLE timeline that shipped with zero tests. These tests
// use a fresh track per case and filter queries by that track id, so the accumulating global
// state does not cross-contaminate. They pin the three correctness bugs found on audit
// (split iterator-invalidation UB, ripple-delete shifting earlier clips, transition-alpha
// divide-by-zero) plus the core timeline operations.

namespace {

using namespace nexus::animation;

// Fetch a clip by id from the current global state (or a zeroed clip with id 0 if absent).
VideoClip clipById(uint32_t id) {
    for (const auto& c : VideoEditor::getClipsInRange(-1e9f, 1e9f))
        if (c.id == id) return c;
    return VideoClip{};
}

std::vector<VideoClip> clipsOnTrack(uint32_t trackId) {
    std::vector<VideoClip> out;
    for (const auto& c : VideoEditor::getClipsInRange(-1e9f, 1e9f))
        if (c.trackId == trackId) out.push_back(c);
    std::sort(out.begin(), out.end(),
              [](const VideoClip& a, const VideoClip& b) { return a.timelineStart < b.timelineStart; });
    return out;
}

VideoClip makeClip(uint32_t track, float tStart, float tEnd, float sStart, float sEnd) {
    VideoClip c;
    c.trackId = track;
    c.timelineStart = tStart;
    c.timelineEnd = tEnd;
    c.sourceStart = sStart;
    c.sourceEnd = sEnd;
    return c;
}

} // namespace

TEST(VideoEditor, SplitClipDividesTimelineAndSourceConsistently) {
    const uint32_t tr = VideoEditor::addTrack("split");
    const uint32_t id = VideoEditor::addClip(makeClip(tr, 0.f, 10.f, 0.f, 10.f));

    VideoEditor::splitClip(id, 4.f);

    auto clips = clipsOnTrack(tr);
    ASSERT_EQ(clips.size(), 2u);
    // Left half keeps the original id and start; right half is new.
    EXPECT_FLOAT_EQ(clips[0].timelineStart, 0.f);
    EXPECT_FLOAT_EQ(clips[0].timelineEnd, 4.f);
    EXPECT_FLOAT_EQ(clips[0].sourceStart, 0.f);
    EXPECT_FLOAT_EQ(clips[0].sourceEnd, 4.f);
    EXPECT_FLOAT_EQ(clips[1].timelineStart, 4.f);
    EXPECT_FLOAT_EQ(clips[1].timelineEnd, 10.f);
    EXPECT_FLOAT_EQ(clips[1].sourceStart, 4.f);
    EXPECT_FLOAT_EQ(clips[1].sourceEnd, 10.f);
    // The two halves tile the original span exactly, with no gap or overlap.
    EXPECT_FLOAT_EQ(clips[0].timelineEnd, clips[1].timelineStart);
}

// Stresses the reallocation path that the old range-for + push_back hit as undefined
// behaviour: many splits force the clip vector to grow repeatedly. Under ASan the old code
// would trip; here we assert the structure stays consistent.
TEST(VideoEditor, RepeatedSplitsStayConsistentUnderReallocation) {
    const uint32_t tr = VideoEditor::addTrack("many-splits");
    uint32_t id = VideoEditor::addClip(makeClip(tr, 0.f, 100.f, 0.f, 100.f));

    for (float t = 10.f; t < 100.f; t += 10.f) {
        // Split whichever clip currently contains t.
        for (const auto& c : clipsOnTrack(tr)) {
            if (t > c.timelineStart && t < c.timelineEnd) { VideoEditor::splitClip(c.id, t); break; }
        }
    }

    auto clips = clipsOnTrack(tr);
    ASSERT_EQ(clips.size(), 10u);  // 9 cuts -> 10 pieces
    for (size_t i = 0; i < clips.size(); ++i) {
        EXPECT_LT(clips[i].timelineStart, clips[i].timelineEnd) << "piece " << i << " degenerate";
        if (i > 0) EXPECT_FLOAT_EQ(clips[i - 1].timelineEnd, clips[i].timelineStart) << "gap/overlap at " << i;
    }
    (void)id;
}

// Ripple-delete closes the gap by pulling only LATER clips left; clips before the deleted one
// must not move. The old code shifted every clip on the track with timelineStart > 0.
TEST(VideoEditor, RippleDeleteClosesGapAndLeavesEarlierClipsPut) {
    const uint32_t tr = VideoEditor::addTrack("ripple");
    const uint32_t a = VideoEditor::addClip(makeClip(tr, 0.f, 5.f, 0.f, 5.f));
    const uint32_t b = VideoEditor::addClip(makeClip(tr, 5.f, 10.f, 0.f, 5.f));
    const uint32_t c = VideoEditor::addClip(makeClip(tr, 10.f, 15.f, 0.f, 5.f));

    VideoEditor::rippleDelete(b);

    EXPECT_EQ(clipById(b).id, 0u) << "deleted clip should be gone";
    // a started before the deletion -> unchanged.
    const VideoClip ca = clipById(a);
    EXPECT_FLOAT_EQ(ca.timelineStart, 0.f);
    EXPECT_FLOAT_EQ(ca.timelineEnd, 5.f);
    // c came after -> pulled left by the deleted 5s to close the gap.
    const VideoClip cc = clipById(c);
    EXPECT_FLOAT_EQ(cc.timelineStart, 5.f);
    EXPECT_FLOAT_EQ(cc.timelineEnd, 10.f);
}

// The specific regression: deleting a clip must never drag an earlier clip to negative time.
TEST(VideoEditor, RippleDeleteNeverPushesEarlierClipNegative) {
    const uint32_t tr = VideoEditor::addTrack("ripple-neg");
    const uint32_t early = VideoEditor::addClip(makeClip(tr, 2.f, 5.f, 0.f, 3.f));
    const uint32_t late = VideoEditor::addClip(makeClip(tr, 5.f, 10.f, 0.f, 5.f));

    VideoEditor::rippleDelete(late);

    const VideoClip e = clipById(early);
    EXPECT_FLOAT_EQ(e.timelineStart, 2.f) << "an earlier clip must not be shifted by a later deletion";
    EXPECT_FLOAT_EQ(e.timelineEnd, 5.f);
    EXPECT_GE(e.timelineStart, 0.f);
}

TEST(VideoEditor, TransitionAlphaRampsLinearlyAndClamps) {
    const uint32_t tr = VideoEditor::addTrack("trans");
    const uint32_t from = VideoEditor::addClip(makeClip(tr, 0.f, 10.f, 0.f, 10.f));
    const uint32_t to = VideoEditor::addClip(makeClip(tr, 8.f, 18.f, 0.f, 10.f));
    Transition t;
    t.fromClip = from;
    t.toClip = to;
    t.duration = 2.f;  // transition over [8, 10]
    VideoEditor::addTransition(t);

    EXPECT_FLOAT_EQ(VideoEditor::getTransitionAlpha(from, to, 7.f), 0.f);   // before
    EXPECT_FLOAT_EQ(VideoEditor::getTransitionAlpha(from, to, 8.f), 0.f);   // start
    EXPECT_FLOAT_EQ(VideoEditor::getTransitionAlpha(from, to, 9.f), 0.5f);  // midpoint
    EXPECT_FLOAT_EQ(VideoEditor::getTransitionAlpha(from, to, 10.f), 1.f);  // end
    EXPECT_FLOAT_EQ(VideoEditor::getTransitionAlpha(from, to, 12.f), 1.f);  // after
}

// A zero-duration transition must not divide by zero.
TEST(VideoEditor, ZeroDurationTransitionIsFiniteStep) {
    const uint32_t tr = VideoEditor::addTrack("trans0");
    const uint32_t from = VideoEditor::addClip(makeClip(tr, 0.f, 10.f, 0.f, 10.f));
    const uint32_t to = VideoEditor::addClip(makeClip(tr, 5.f, 15.f, 0.f, 10.f));
    Transition t;
    t.fromClip = from;
    t.toClip = to;
    t.duration = 0.f;
    VideoEditor::addTransition(t);

    const float before = VideoEditor::getTransitionAlpha(from, to, 4.f);
    const float after = VideoEditor::getTransitionAlpha(from, to, 6.f);
    EXPECT_TRUE(std::isfinite(before)) << "zero-duration transition produced non-finite alpha";
    EXPECT_TRUE(std::isfinite(after));
    EXPECT_FLOAT_EQ(before, 0.f);
    EXPECT_FLOAT_EQ(after, 1.f);
}

TEST(VideoEditor, TrimAndMoveAdjustTimelineSpanCorrectly) {
    const uint32_t tr = VideoEditor::addTrack("trim-move");
    const uint32_t id = VideoEditor::addClip(makeClip(tr, 3.f, 13.f, 0.f, 10.f));

    // Trim to a 4s source window -> timeline end = start + 4.
    VideoEditor::trimClip(id, 2.f, 6.f);
    VideoClip c = clipById(id);
    EXPECT_FLOAT_EQ(c.sourceStart, 2.f);
    EXPECT_FLOAT_EQ(c.sourceEnd, 6.f);
    EXPECT_FLOAT_EQ(c.timelineEnd - c.timelineStart, 4.f);

    // Move preserves duration.
    const float dur = c.timelineEnd - c.timelineStart;
    VideoEditor::moveClip(id, 20.f);
    c = clipById(id);
    EXPECT_FLOAT_EQ(c.timelineStart, 20.f);
    EXPECT_FLOAT_EQ(c.timelineEnd - c.timelineStart, dur);
}

TEST(VideoEditor, ClipQueriesRespectHalfOpenInterval) {
    const uint32_t tr = VideoEditor::addTrack("query");
    const uint32_t id = VideoEditor::addClip(makeClip(tr, 100.f, 110.f, 0.f, 10.f));

    auto at = [&](float t) {
        for (const auto& c : VideoEditor::getClipsAtTime(t))
            if (c.id == id) return true;
        return false;
    };
    EXPECT_TRUE(at(100.f));   // inclusive start
    EXPECT_TRUE(at(105.f));
    EXPECT_FALSE(at(110.f));  // exclusive end
    EXPECT_FALSE(at(99.f));
}

TEST(VideoEditor, SnapToClipSnapsWithinThresholdOnly) {
    const uint32_t tr = VideoEditor::addTrack("snap");
    (void)VideoEditor::addClip(makeClip(tr, 50.f, 60.f, 0.f, 10.f));
    const uint32_t mover = VideoEditor::addClip(makeClip(tr, 0.f, 5.f, 0.f, 5.f));

    float t = 49.7f;  // within 0.5 of the edge at 50
    VideoEditor::snapToClip(mover, t);
    EXPECT_FLOAT_EQ(t, 50.f) << "should snap to the nearby clip edge";

    float far = 45.f;  // beyond threshold
    VideoEditor::snapToClip(mover, far);
    EXPECT_FLOAT_EQ(far, 45.f) << "should not snap when no edge is within threshold";
}
