#include <gtest/gtest.h>
#include "nexus/renderpass/RenderPassManager.h"
using namespace nexus::renderpass;
TEST(RenderPassManager,CreateAOV){RenderPassManager mgr;auto& buf=mgr.createAOV(AOVType::Beauty);EXPECT_TRUE(buf.valid());EXPECT_EQ(buf.width,1920u);}
TEST(RenderPassManager,QueryAOV){RenderPassManager mgr;mgr.createAOV(AOVType::Albedo);auto* a=mgr.aov(AOVType::Albedo);EXPECT_NE(a,nullptr);}
TEST(RenderPassManager,Composite){RenderPassManager mgr;auto& b=mgr.createAOV(AOVType::Beauty);for(size_t i=0;i<10;++i)b.data[i]=0.5f;auto c=mgr.composite({b});EXPECT_NEAR(c[0],0.5f,0.01f);}
