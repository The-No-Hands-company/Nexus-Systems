#include <gtest/gtest.h>
#include "nexus/texture/TextureImage.h"
#include "nexus/texture/MipmapGenerator.h"
#include "nexus/texture/TextureAtlas.h"
using namespace nexus::texture;
TEST(TextureImage,CreateClear){TextureImage img;img.desc.width=64;img.desc.height=64;img.clear(255,0,0);EXPECT_TRUE(img.valid());auto* p=img.pixelData(10,10);ASSERT_NE(p,nullptr);EXPECT_EQ(p[0],255);}
TEST(TextureImage,SetPixel){TextureImage img;img.desc.width=32;img.desc.height=32;img.clear();img.setPixel(5,5,128,64,32);auto* p=img.pixelData(5,5);EXPECT_EQ(p[0],128);EXPECT_EQ(p[1],64);EXPECT_EQ(p[2],32);}
TEST(TextureImage,MipGeneration){TextureImage img;img.desc.width=64;img.desc.height=64;img.clear(255,255,255);img.generateMipLevels(4);EXPECT_EQ(img.desc.mipLevels,4u);}
TEST(TextureImage,Resize){TextureImage img;img.desc.width=16;img.desc.height=16;img.clear(255,0,0);img.resize(32,32);EXPECT_EQ(img.desc.width,32u);}
TEST(MipmapGenerator,GenerateChain){TextureImage img;img.desc.width=32;img.desc.height=32;img.clear(200,100,50);auto chain=MipmapGenerator::generateMipChain(img);EXPECT_GE(chain.size(),1u);}
TEST(TextureAtlas,Pack){TextureAtlas atlas(256,256);TextureImage a;a.desc.width=32;a.desc.height=32;a.clear(255,0,0);TextureImage b;b.desc.width=16;b.desc.height=16;b.clear(0,255,0);atlas.addImage(a,"A");atlas.addImage(b,"B");EXPECT_TRUE(atlas.pack());EXPECT_EQ(atlas.entries().size(),2u);}
