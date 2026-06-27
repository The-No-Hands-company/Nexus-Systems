#include <gtest/gtest.h>
#include "nexus/light/LightManager.h"
using namespace nexus::light;
TEST(Light,Add){LightManager lm;auto id=lm.addLight(LightConfig{});EXPECT_GT(id,0u);EXPECT_EQ(lm.count(),1u);}
TEST(Light,Eval){LightManager lm;LightConfig c;c.type=LightType::Point;c.position=Vec3{0,0,0};c.intensity=100;lm.addLight(c);auto e=lm.evaluate(Vec3{1,0,0},1);EXPECT_GT(e.x,0.f);}
