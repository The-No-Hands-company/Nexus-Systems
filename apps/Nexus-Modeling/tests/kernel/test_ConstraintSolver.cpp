#include <gtest/gtest.h>
#include "nexus/core/constraint/ConstraintSolver.h"
using namespace nexus::constraint;
TEST(Constraint,Pin){ConstraintSolver cs;cs.addConstraint({ConstraintKind::Pin,Vec3{5,5,5}});auto r=cs.solve({{0,0,0}});EXPECT_FLOAT_EQ(r[0].x,5.f);}
TEST(Constraint,Distance){ConstraintSolver cs;ConstraintDef d;d.kind=ConstraintKind::Distance;d.source=0;d.targetId=1;d.value=3;d.stiffness=1;cs.addConstraint(d);auto r=cs.solve({{0,0,0},{5,0,0}});}
