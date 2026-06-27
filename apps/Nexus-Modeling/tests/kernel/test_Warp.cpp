#include <gtest/gtest.h>
#include "nexus/warp/WarpEngine.h"
#include "nexus/geometry/Mesh.h"
using namespace nexus::warp;
using namespace nexus;
TEST(WarpEngine,Bend){auto m=geometry::primitives::makeBox(2,4,2);auto r=WarpEngine::applyBend(m,0.5f,Vec3{1,0,0});EXPECT_GT(r.topology().faceCount(),0u);}
TEST(WarpEngine,Twist){auto m=geometry::primitives::makeBox(2,4,2);auto r=WarpEngine::applyTwist(m,0.3f,Vec3{0,1,0});EXPECT_GT(r.topology().faceCount(),0u);}
TEST(WarpEngine,Morph){auto a=geometry::primitives::makeBox(1,1,1);auto b=geometry::primitives::makeSphere(1);auto r=WarpEngine::applyMorph(a,b,0.5f);EXPECT_GT(r.topology().faceCount(),0u);}
