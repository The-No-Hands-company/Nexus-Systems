#include <gtest/gtest.h>
#include "nexus/cloth/ClothSimulation.h"
using namespace nexus::cloth;
TEST(Cloth,Init){ClothSimulation cs;cs.initialize();EXPECT_GT(cs.particles().size(),0u);}
TEST(Cloth,Step){ClothSimulation cs;cs.initialize();cs.step(0.016f);EXPECT_GT(cs.particles().size(),0u);}
TEST(Cloth,ToMesh){ClothSimulation cs;cs.initialize();auto m=cs.toMesh();EXPECT_GT(m.topology().faceCount(),0u);}
