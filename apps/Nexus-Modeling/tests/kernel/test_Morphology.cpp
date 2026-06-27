#include <gtest/gtest.h>
#include "nexus/rendering/morphology/MorphologyOps.h"
#include "nexus/geometry/Mesh.h"
using namespace nexus::morphology;
using namespace nexus;
TEST(Morph,Dilate){auto m=geometry::primitives::makeBox(1,1,1);auto r=MorphologyOps::dilate(m,1);EXPECT_GT(r.topology().faceCount(),0u);}
TEST(Morph,Smooth){auto m=geometry::primitives::makeBox(1,1,1);auto r=MorphologyOps::smooth(m);EXPECT_GT(r.topology().faceCount(),0u);}
