#include <gtest/gtest.h>
#include "nexus/animation/motion/MotionRetargeter.h"
using namespace nexus::motion;
TEST(Skeleton,AddJoint){Skeleton s;s.addJoint("root",0,Vec3{0,0,0});s.addJoint("child",0,Vec3{0,1,0});EXPECT_EQ(s.jointCount(),2u);}
TEST(Skeleton,UpdateWorld){Skeleton s;auto root=s.addJoint("root",0,Vec3{0,0,0});s.addJoint("child",root,Vec3{0,1,0});s.updateWorldTransforms();EXPECT_FLOAT_EQ(s.joint(1)->worldPosition.y,1.f);}
TEST(MotionRetargeter,Humanoid){auto skel=MotionRetargeter::createHumanoidSkeleton();EXPECT_GE(skel.jointCount(),5u);}
TEST(MotionRetargeter,Blend){MotionClip a,b;a.duration=1;a.framerate=30;a.jointPositions={{{0,0,0}}};b.duration=1;b.framerate=30;b.jointPositions={{{1,1,1}}};auto c=MotionRetargeter::blend(a,b,0.5f);EXPECT_NEAR(c.jointPositions[0][0].x,0.5f,0.01f);}
TEST(MotionRetargeter,Retarget){Skeleton src=MotionRetargeter::createHumanoidSkeleton();Skeleton dst=MotionRetargeter::createHumanoidSkeleton();MotionClip clip;clip.duration=1;clip.framerate=30;clip.jointPositions={std::vector<Vec3>(src.jointCount(),Vec3{1,1,1})};auto r=MotionRetargeter::retarget(clip,src,dst);EXPECT_EQ(r.jointPositions.size(),1u);}
