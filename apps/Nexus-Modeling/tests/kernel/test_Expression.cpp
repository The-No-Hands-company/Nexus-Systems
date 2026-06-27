#include <gtest/gtest.h>
#include "nexus/expression/ExpressionEngine.h"
using namespace nexus::expression;
TEST(ExpressionEngine,Constant){ExpressionEngine e;EXPECT_TRUE(e.compile("42"));EXPECT_DOUBLE_EQ(e.evaluate(),42);}
TEST(ExpressionEngine,Addition){ExpressionEngine e;EXPECT_TRUE(e.compile("2+3"));EXPECT_DOUBLE_EQ(e.evaluate(),5);}
TEST(ExpressionEngine,Variables){ExpressionEngine e;e.define("x",10);e.define("y",20);EXPECT_TRUE(e.compile("x+y"));EXPECT_DOUBLE_EQ(e.evaluate(),30);}
TEST(ExpressionEngine,Functions){ExpressionEngine e;EXPECT_TRUE(e.compile("sin(0)"));EXPECT_NEAR(e.evaluate(),0,0.01);EXPECT_TRUE(e.compile("cos(0)"));EXPECT_NEAR(e.evaluate(),1,0.01);EXPECT_TRUE(e.compile("sqrt(16)"));EXPECT_DOUBLE_EQ(e.evaluate(),4);}
TEST(ExpressionEngine,Precedence){ExpressionEngine e;EXPECT_TRUE(e.compile("2+3*4"));EXPECT_DOUBLE_EQ(e.evaluate(),14);}
TEST(ExpressionEngine,Nested){ExpressionEngine e;EXPECT_TRUE(e.compile("(2+3)*4"));EXPECT_DOUBLE_EQ(e.evaluate(),20);}
