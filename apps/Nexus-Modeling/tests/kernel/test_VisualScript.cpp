#include <gtest/gtest.h>
#include "nexus/scripting/VisualScript.h"
using namespace nexus::scripting;
TEST(VisualScript,AddNode){VisualScript vs;auto id=vs.addNode(ScriptNodeType::Float);EXPECT_GT(id,0u);EXPECT_EQ(vs.graph().nodes.size(),1u);}
TEST(VisualScript,ConnectNodes){VisualScript vs;auto a=vs.addNode(ScriptNodeType::Float);auto b=vs.addNode(ScriptNodeType::Print);vs.connect(a,0,b,0);EXPECT_EQ(vs.graph().edges.size(),1u);}
TEST(VisualScript,Execute){VisualScript vs;vs.addNode(ScriptNodeType::Float);vs.execute();}
TEST(VisualScript,Clear){VisualScript vs;vs.addNode(ScriptNodeType::Float);vs.clear();EXPECT_EQ(vs.graph().nodes.size(),0u);}
