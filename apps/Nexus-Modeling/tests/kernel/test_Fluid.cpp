#include <gtest/gtest.h>
#include "nexus/fluid/FluidSimulation.h"
using namespace nexus::fluid;
TEST(Fluid,Init){FluidSimulation fs;fs.initialize();EXPECT_GT(fs.particles().size(),0u);}
TEST(Fluid,Step){FluidSimulation fs;fs.initialize();fs.step(0.016f);EXPECT_GT(fs.particles().size(),0u);}
