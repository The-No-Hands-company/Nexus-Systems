#include <gtest/gtest.h>
#include <nexus/render/SceneGraph.h>
#include <nexus/sim/SimulationCoupling.h>
#include <nexus/sim/SimulationCore.h>

using namespace nexus;

TEST(SimulationCoupling, AppliesBoundBodyPositionToSceneNode)
{
    render::SceneGraph scene;
    render::Node* node = scene.createNode("body", &scene.root());
    ASSERT_NE(node, nullptr);

    sim::SimulationSceneCoupling coupling(scene);
    sim::SimulationSceneBinding binding{};
    binding.simBodyId = 1;
    binding.sceneNodeId = node->id();
    coupling.setBindings(std::span{&binding, 1});

    SimState state;
    SimBodySnapshot snapshot{};
    snapshot.id = 1;
    snapshot.position = {1.0f, 2.0f, 3.0f};
    state.bodies.push_back(snapshot);

    coupling.applyState(state);

    const auto translation = node->localTransform().translation;
    EXPECT_FLOAT_EQ(translation.x, 1.0f);
    EXPECT_FLOAT_EQ(translation.y, 2.0f);
    EXPECT_FLOAT_EQ(translation.z, 3.0f);
}

TEST(SimulationCoupling, IgnoresUnboundBodies)
{
    render::SceneGraph scene;
    render::Node* node = scene.createNode("body", &scene.root());
    ASSERT_NE(node, nullptr);

    sim::SimulationSceneCoupling coupling(scene);
    sim::SimulationSceneBinding binding{};
    binding.simBodyId = 2;
    binding.sceneNodeId = node->id();
    coupling.setBindings(std::span{&binding, 1});

    SimState state;
    SimBodySnapshot snapshot{};
    snapshot.id = 1;
    snapshot.position = {4.0f, 5.0f, 6.0f};
    state.bodies.push_back(snapshot);

    const auto beforeTranslation = node->localTransform().translation;
    coupling.applyState(state);
    const auto afterTranslation = node->localTransform().translation;

    EXPECT_EQ(afterTranslation, beforeTranslation);
}
