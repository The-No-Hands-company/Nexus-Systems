#include <gtest/gtest.h>
#include "nexus/bake/TextureBaker.h"
#include "nexus/geometry/Mesh.h"
using namespace nexus::bake;
using namespace nexus;
TEST(Bake,Basic){TextureBaker tb;auto lp=geometry::primitives::makeBox(1,1,1);auto hp=geometry::primitives::makeBox(1,1,1);auto r=tb.bake(lp,hp);EXPECT_GT(r.albedo.size(),0u);}
