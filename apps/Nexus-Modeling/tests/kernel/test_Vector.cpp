#include <gtest/gtest.h>
#include "nexus/vector/VectorDocument.h"
using namespace nexus::vector;
TEST(VectorDocument,AddLayer){VectorDocument doc;doc.addLayer(VectorLayer{});EXPECT_EQ(doc.layers().size(),1u);}
TEST(VectorDocument,RectPath){VectorDocument doc;auto p=doc.createRectPath(0,0,100,50);EXPECT_EQ(p.commands.size(),5u);EXPECT_TRUE(p.closed);}
TEST(VectorDocument,EllipsePath){VectorDocument doc;auto p=doc.createEllipsePath(0,0,50,30,16);EXPECT_GT(p.commands.size(),2u);}
TEST(VectorDocument,StarPath){VectorDocument doc;auto p=doc.createStarPath(0,0,50,25,5);EXPECT_GT(p.commands.size(),2u);}
