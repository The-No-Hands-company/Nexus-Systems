#include <gtest/gtest.h>
#include "nexus/groom/GroomSystem.h"
using namespace nexus::groom;
TEST(Groom,Gen){GroomConfig c;c.strandCount=50;GroomSystem gs(c);gs.generate();EXPECT_EQ(gs.strands().size(),50u);}
TEST(Groom,Comb){GroomConfig c;c.strandCount=10;GroomSystem gs(c);gs.generate();gs.comb(Vec3{0,0,0},1,Vec3{1,0,0},0.5f);}
