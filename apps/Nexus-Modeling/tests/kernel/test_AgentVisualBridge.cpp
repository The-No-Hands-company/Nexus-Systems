// ─────────────────────────────────────────────────────────────────────────────
//  Test: Agent Visual Bridge — pixel-to-entity resolution
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/agent/AgentVisualBridge.h>
#include <nexus/cad/CadCommand.h>
#include <nexus/geometry/HalfEdgeMesh.h>
#include <nexus/geometry/Mesh.h>

#include <gtest/gtest.h>

using namespace nexus::agent;
using namespace nexus::geometry;

TEST(AgentVisualBridge, ScreenToWorldRayConsistency)
{
    AgentVisualBridge bridge;
    VisualQuery query;
    query.screenX = 960.0f;
    query.screenY = 540.0f;
    query.screenWidth = 1920.0f;
    query.screenHeight = 1080.0f;

    Vec3 origin, direction;
    direction = bridge.screenToWorldRay(query, origin, direction);
    EXPECT_GT(direction.length(), 0.5f);
}

TEST(AgentVisualBridge, RayCastFaceOnBox)
{
    AgentVisualBridge bridge;
    auto mesh = primitives::makeBox(2.0f, 2.0f, 2.0f);

    Vec3 origin{0.0f, 0.0f, 5.0f};
    Vec3 direction{0.0f, 0.0f, -1.0f};

    auto hit = bridge.rayCastFace(origin, direction, mesh);
    EXPECT_TRUE(hit.valid);
    EXPECT_NE(hit.faceId, ~0u);
    EXPECT_GT(hit.depth, 0.0f);
}

TEST(AgentVisualBridge, RayCastMissesEmptySpace)
{
    AgentVisualBridge bridge;
    auto mesh = primitives::makeBox(2.0f, 2.0f, 2.0f);

    Vec3 origin{10.0f, 10.0f, 10.0f};
    Vec3 direction{1.0f, 0.0f, 0.0f};

    auto hit = bridge.rayCastFace(origin, direction, mesh);
    EXPECT_FALSE(hit.valid);
}

TEST(AgentVisualBridge, QueryReturnsHitOnCenter)
{
    AgentVisualBridge bridge;
    auto mesh = primitives::makeBox(2.0f, 2.0f, 2.0f);

    VisualQuery query;
    query.screenX = 960.0f;
    query.screenY = 540.0f;
    query.screenWidth = 1920.0f;
    query.screenHeight = 1080.0f;

    auto feedback = bridge.query(query, mesh);
    EXPECT_TRUE(feedback.valid) << "Expected ray to hit the box from default camera";
}

TEST(AgentVisualBridge, QueryRegion)
{
    AgentVisualBridge bridge;
    auto mesh = primitives::makeBox(2.0f, 2.0f, 2.0f);

    VisualRegion region;
    region.minX = 0.3f;
    region.minY = 0.3f;
    region.maxX = 0.7f;
    region.maxY = 0.7f;

    auto feedback = bridge.queryRegion(region, mesh);
    EXPECT_TRUE(feedback.valid);
}

TEST(AgentVisualBridge, ObjectIdBufferQuery)
{
    AgentVisualBridge bridge;
    auto mesh = primitives::makeBox(2.0f, 2.0f, 2.0f);

    std::vector<uint32_t> idBuffer(1920 * 1080, ~0u);
    idBuffer[540 * 1920 + 960] = 3u;

    VisualQuery query;
    query.screenX = 960.0f;
    query.screenY = 540.0f;
    query.screenWidth = 1920.0f;
    query.screenHeight = 1080.0f;

    auto feedback = bridge.queryByIdBuffer(query, mesh, idBuffer, 1920, 1080);
    EXPECT_TRUE(feedback.valid);
    EXPECT_EQ(feedback.hits[0].faceId, 3u);
}

TEST(AgentVisualBridge, SetCamera)
{
    AgentVisualBridge bridge;
    nexus::render::Camera cam;
    cam.lookAt(Vec3{0, 5, 10}, Vec3{0, 0, 0}, Vec3{0, 1, 0});
    bridge.setCamera(cam);
    EXPECT_FLOAT_EQ(bridge.camera().position().y, 5.0f);
}
