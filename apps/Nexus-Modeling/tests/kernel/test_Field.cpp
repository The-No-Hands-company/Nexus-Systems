#include <gtest/gtest.h>
#include "nexus/field/FieldSystem.h"
using namespace nexus::field;
TEST(Field,Create){FieldConfig c;auto f=FieldSystem::createField(c);EXPECT_GT(f.values.size(),0u);}
TEST(Field,Sphere){FieldConfig c;auto f=FieldSystem::createField(c);FieldSystem::addSphere(f,Vec3{0,0,0},2,1);EXPECT_GT(f.sample(0,0,0),0.f);}
