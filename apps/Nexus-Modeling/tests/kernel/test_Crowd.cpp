#include <gtest/gtest.h>
#include "nexus/animation/crowd/CrowdSystem.h"
using namespace nexus::crowd;
TEST(CrowdSystem,Initialize){CrowdSystem cs;cs.initialize();EXPECT_EQ(cs.agents().size(),50u);}
TEST(CrowdSystem,UpdateMovesAgents){CrowdSystem cs;cs.initialize();for(uint32_t i=0;i<50;++i)cs.setGoalForAgent(i+1,Vec3{100,0,100});cs.update(1.f);float moved=false;for(auto& a:cs.agents())if(a.speed>0)moved=true;EXPECT_TRUE(moved);}
TEST(CrowdSystem,Obstacles){CrowdSystem cs;cs.initialize();cs.addObstacle(Vec3{0,0,0},5);EXPECT_EQ(cs.obstacles().size(),1u);}
