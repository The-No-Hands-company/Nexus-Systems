#include <gtest/gtest.h>
#include "nexus/color/ColorGrade.h"
using namespace nexus::color;
TEST(Color,Exposure){auto c=ColorGrade::applyExposure(Vec3{0.5f,0.5f,0.5f},1);EXPECT_GT(c.x,0.5f);}
TEST(Color,SRGB){auto c=ColorGrade::srgbToLinear(Vec3{0.5f,0.5f,0.5f});EXPECT_LT(c.x,0.5f);auto r=ColorGrade::linearToSRGB(c);EXPECT_NEAR(r.x,0.5f,0.01f);}
TEST(Color,Tonemap){auto c=ColorGrade::acesTonemap(Vec3{1,1,1});EXPECT_LT(c.x,1.f);}
