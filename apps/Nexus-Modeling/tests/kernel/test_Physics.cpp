#include <gtest/gtest.h>
#include "nexus/physics/PhysicsWorld.h"
using namespace nexus::physics;
TEST(PhysicsWorld,AddBody){PhysicsWorld w;auto id=w.addRigidBody(RigidBody{});EXPECT_GT(id,0u);EXPECT_EQ(w.bodyCount(),1u);}
TEST(PhysicsWorld,StepMovesBodies){PhysicsWorld w;RigidBody b;b.position=Vec3{0,10,0};b.velocity=Vec3{0,-1,0};w.addRigidBody(b);w.step(1.f);auto* body=w.body(1);ASSERT_NE(body,nullptr);EXPECT_LT(body->position.y,10.f);}
TEST(PhysicsWorld,CollisionDetected){PhysicsWorld w;RigidBody a;a.position=Vec3{0,0,0};a.id=1;RigidBody b;b.position=Vec3{0.5f,0,0};b.id=2;w.addRigidBody(a);w.addRigidBody(b);auto contacts=w.collide();EXPECT_GT(contacts.size(),0u);}
TEST(PhysicsWorld,SoftBody){PhysicsWorld w;SoftBody sb;sb.positions={{0,0,0},{0,1,0}};sb.prevPositions=sb.positions;sb.masses={1,1};sb.springs={{0,1}};sb.restLengths={1};w.addSoftBody(sb);w.step(0.016f);}
TEST(PhysicsWorld,Clear){PhysicsWorld w;w.addRigidBody(RigidBody{});w.clear();EXPECT_EQ(w.bodyCount(),0u);}
