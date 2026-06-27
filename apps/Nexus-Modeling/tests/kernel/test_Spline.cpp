#include <gtest/gtest.h>
#include "nexus/spline/Spline.h"
using namespace nexus::spline;
TEST(Spline,Eval){Spline s;s.addPoint(Vec3{0,0,0});s.addPoint(Vec3{1,1,1});auto p=s.evaluate(0.5f);EXPECT_GT(p.x,0.f);}
TEST(Spline,Length){Spline s;s.addPoint(Vec3{0,0,0});s.addPoint(Vec3{3,4,0});EXPECT_NEAR(s.length(),5.f,0.5f);}
