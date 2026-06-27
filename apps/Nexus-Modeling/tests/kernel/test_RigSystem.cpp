#include <gtest/gtest.h>
#include "nexus/animation/rig/RigSystem.h"
using namespace nexus::rig;
TEST(Rig,AddBone){RigSystem rs;auto id=rs.addBone("root",0,Vec3{0,0,0},Vec3{0,1,0});EXPECT_GT(id,0u);EXPECT_EQ(rs.boneCount(),1u);}
TEST(Rig,IK){RigSystem rs;auto root=rs.addBone("r",0,Vec3{0,0,0},Vec3{0,1,0});rs.addBone("c",root,Vec3{0,1,0},Vec3{0,2,0});rs.addIKChain(IKChain{"ik",root,2});rs.solveIK(0,Vec3{0,3,0});}
