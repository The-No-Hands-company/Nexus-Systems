#include <gtest/gtest.h>
#include "nexus/animation/sequence/Timeline.h"
using namespace nexus::sequence;
TEST(Timeline,AddTrack){Timeline t;t.addTrack("Video",0,10);EXPECT_EQ(t.tracks().size(),1u);}
TEST(Timeline,AddClip){Timeline t;t.addTrack("V",0,10);TimelineClip c;c.name="C1";c.trackId=1;t.addClip(c);EXPECT_EQ(t.clips().size(),1u);}
TEST(Timeline,Playhead){Timeline t;t.setPlayhead(5);EXPECT_FLOAT_EQ(t.playhead(),5);t.stepForward();EXPECT_GT(t.playhead(),5);t.stepBackward();EXPECT_LT(t.playhead(),t.playhead()+1);}
TEST(Timeline,Marker){Timeline t;t.addMarker({1,"start"});EXPECT_EQ(t.markers().size(),1u);}
TEST(Timeline,Clear){Timeline t;t.addTrack("T",0,1);t.clear();EXPECT_EQ(t.tracks().size(),0u);}
