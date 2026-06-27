#include <gtest/gtest.h>
#include "nexus/animation/hair/HairSystem.h"
using namespace nexus::hair;
TEST(HairSystem,Generate){HairConfig c;c.strandCount=100;c.pointsPerStrand=4;HairSystem hs(c);hs.generate();EXPECT_EQ(hs.strands().size(),100u);}
TEST(HairSystem,Simulate){HairSystem hs;hs.generate();hs.simulate(0.016f);EXPECT_GT(hs.strands().size(),0u);}
TEST(HairSystem,ToMesh){HairSystem hs;hs.generate();auto m=hs.toMesh();EXPECT_GT(m.topology().faceCount(),0u);}
TEST(HairSystem,Reset){HairSystem hs;hs.generate();hs.reset();EXPECT_EQ(hs.strands().size(),0u);}
