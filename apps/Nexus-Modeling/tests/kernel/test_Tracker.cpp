#include <gtest/gtest.h>
#include "nexus/tracking/Tracker.h"
using namespace nexus::tracking;
TEST(Tracker2D,AddMarker){Tracker2D t;TrackMarker m;m.position={1,2,3};m.size=0.1f;t.addMarker(m);EXPECT_EQ(t.markers().size(),1u);}
TEST(Tracker2D,Track){Tracker2D t;TrackMarker m;m.position={0,0,0};m.size=0.2f;t.addMarker(m);std::vector<Vec3> frame={Vec3{0.01f,0.01f,0}};t.track(frame);EXPECT_NEAR(t.markers()[0].position.x,0.01f,0.1f);}
TEST(Tracker2D,SolvePnP){Tracker2D t;TrackMarker m;m.position={0,0,0};t.addMarker(m);auto r=t.solvePnP({Vec3{1,2,3}});EXPECT_TRUE(r.success);}
TEST(Tracker3D,AddTrackPoint){Tracker3D t;t.addTrackPoint({0,0,0},1);EXPECT_EQ(t.points().size(),1u);}
TEST(Tracker3D,UpdateTrack){Tracker3D t;t.addTrackPoint({0,0,0},1);EXPECT_TRUE(t.updateTrack(1,{1,2,3}));EXPECT_FLOAT_EQ(t.points()[0].position.x,1.f);}
TEST(Tracker3D,Predict){Tracker3D t;t.addTrackPoint({0,0,0},1);t.updateTrack(1,{1,0,0});auto pred=t.predictPosition(1,0.5f);EXPECT_GT(pred.x,0.5f);}
