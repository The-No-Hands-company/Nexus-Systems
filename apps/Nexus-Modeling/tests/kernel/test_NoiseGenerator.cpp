#include <gtest/gtest.h>
#include "nexus/noiselib/NoiseGenerator.h"
using namespace nexus::noiselib;
TEST(Noise,Sample){NoiseGenerator ng;float v=ng.sample(1,2,3);EXPECT_GE(v,-1.f);EXPECT_LE(v,1.f);}
TEST(Noise,2D){NoiseGenerator ng;auto d=ng.generate2D(16,16);EXPECT_EQ(d.size(),256u);}
