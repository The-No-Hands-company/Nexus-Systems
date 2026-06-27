#include <gtest/gtest.h>
#include "nexus/terrain/Terrain.h"
using namespace nexus::terrain;
TEST(Terrain,Generate){Terrain t;t.generate();auto hm=t.heightmap();EXPECT_EQ(hm.size(),256u*256u);}
TEST(Terrain,HeightAt){TerrainConfig c;c.resolution=64;c.size=100;Terrain t(c);t.generate();float h=t.heightAt(0,0);EXPECT_GE(h,-c.maxHeight);EXPECT_LE(h,c.maxHeight);}
TEST(Terrain,ToMesh){TerrainConfig c;c.resolution=32;Terrain t(c);t.generate();auto mesh=t.toMesh(1);EXPECT_GT(mesh.topology().faceCount(),0u);}
TEST(Terrain,LOD){Terrain t;t.generate();auto lods=t.buildLODChain();EXPECT_GE(lods.size(),1u);}
TEST(Terrain,Erosion){Terrain t;t.generate();float before=t.heightAt(10,10);t.erode(3,0.1f);float after=t.heightAt(10,10);EXPECT_NE(before,after);}
TEST(Terrain,Smooth){Terrain t;t.generate();float before=t.heightAt(10,10);t.smooth(2);float after=t.heightAt(10,10);EXPECT_NE(before,after);}
