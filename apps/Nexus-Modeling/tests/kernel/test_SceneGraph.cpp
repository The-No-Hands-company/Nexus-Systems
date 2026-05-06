// ─────────────────────────────────────────────────────────────────────────────
//  Tests: SceneGraph
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/render/SceneGraph.h>
#include <gtest/gtest.h>

using namespace nexus::render;

TEST(SceneGraph, RootExistsAfterConstruct)
{
    SceneGraph sg;
    EXPECT_NE(sg.findNode("__root__"), nullptr);
}

TEST(SceneGraph, CreateAndFindByName)
{
    SceneGraph sg;
    auto* node = sg.createNode("mesh_001");
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->name(), "mesh_001");
    EXPECT_EQ(sg.findNode("mesh_001"), node);
}

TEST(SceneGraph, CreateAndFindById)
{
    SceneGraph sg;
    auto* node = sg.createNode("obj");
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(sg.findNode(node->id()), node);
}

TEST(SceneGraph, ParentChildHierarchy)
{
    SceneGraph sg;
    auto* parent = sg.createNode("parent");
    auto* child  = sg.createNode("child", parent);
    EXPECT_EQ(child->parent(), parent);
}

TEST(SceneGraph, TraverseVisitsAllNodes)
{
    SceneGraph sg;
    sg.createNode("a");
    sg.createNode("b");
    sg.createNode("c");

    int count = 0;
    sg.traverse([&](Node&, const Mat4&) { ++count; });
    // root + 3 children
    EXPECT_EQ(count, 4);
}

TEST(SceneGraph, InvisibleNodesNotCollected)
{
    SceneGraph sg;
    auto* n = sg.createNode("hidden");
    n->visible = false;
    // Give it a valid (non-default) vertex buffer handle so collectVisible would include it
    n->mesh.vertexBuffer.id = 1;

    Camera cam;
    cam.setPerspective(120.f, 1.f, 0.01f, 10000.f);
    cam.lookAt({0,0,5}, {0,0,0});

    std::vector<Node*> out;
    sg.collectVisible(cam.frustum(), out);
    for (auto* node : out)
        EXPECT_NE(node->name(), "hidden");
}
