#include <gtest/gtest.h>
#include "nexus/sss/SubsurfaceScattering.h"
using namespace nexus::sss;
TEST(SSS,DiffusionProfile){SubsurfaceScattering sss;auto p=sss.diffusionProfile(0.5f);EXPECT_GT(p.x,0.f);}
TEST(SSS,Evaluate){SubsurfaceScattering sss;auto c=sss.evaluate(Vec3{1,1,1},Vec3{0,1,0},Vec3{0,0,1},0.2f);EXPECT_GT(c.x,0.f);}
TEST(SSS,SamplePoints){SubsurfaceScattering sss;auto pts=sss.generateSamplePoints(Vec3{0,0,0},Vec3{0,1,0},10);EXPECT_EQ(pts.size(),10u);}
