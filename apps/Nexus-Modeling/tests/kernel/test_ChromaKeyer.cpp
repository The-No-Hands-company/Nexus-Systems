#include <gtest/gtest.h>
#include "nexus/keying/ChromaKeyer.h"
using namespace nexus::keying;
TEST(ChromaKeyer,Apply){ChromaKeyer k;std::vector<Vec3> pixels;for(int i=0;i<64*64;++i)pixels.push_back(i%2?Vec3{0,1,0}:Vec3{1,0,0});auto r=k.apply(pixels,64,64);EXPECT_TRUE(r.valid);EXPECT_LT(r.alpha[0],0.5f);EXPECT_GT(r.alpha[1],0.5f);}
TEST(ChromaKeyer,Despill){Vec3 c{0,0.8f,0.2f};auto d=ChromaKeyer::despill(c,Vec3{0,1,0},0.5f);EXPECT_LT(d.y,0.8f);}
TEST(ChromaKeyer,Config){ChromaKeyConfig cfg;cfg.keyColor=Vec3{0,0,1};cfg.threshold=0.5f;ChromaKeyer k(cfg);std::vector<Vec3> px(100,Vec3{1,0,0});auto r=k.apply(px,10,10);EXPECT_TRUE(r.valid);EXPECT_GT(r.alpha[0],0.f);}
