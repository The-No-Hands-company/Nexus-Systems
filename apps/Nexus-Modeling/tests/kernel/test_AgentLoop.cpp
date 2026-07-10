// ─────────────────────────────────────────────────────────────────────────────
//  Test: Agent Loop — integrated orchestrator (planner + visual + recorder)
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/agent/AgentLoop.h>
#include <nexus/agent/AgentSession.h>

#include <gtest/gtest.h>

using namespace nexus::agent;

TEST(AgentLoop, ExecuteIntentWithSingleCandidate)
{
    AgentLoopConfig config;
    config.enablePlanner = false;
    config.enableRecorder = true;
    AgentLoop loop(config);

    AgentCommand cmd;
    cmd.operation = AgentOperation::CreateBox;
    cmd.parameters["width"] = "1.0";
    cmd.parameters["height"] = "2.0";
    cmd.parameters["depth"] = "3.0";

    auto resp = loop.executeIntent("create a box", {cmd});
    EXPECT_TRUE(resp.success);
    EXPECT_GT(loop.status().stepsExecuted, 0u);
}

TEST(AgentLoop, ExecuteIntentWithMultipleCandidates)
{
    AgentLoopConfig config;
    config.enablePlanner = true;
    config.enableRecorder = false;
    AgentLoop loop(config);

    AgentCommand sphere, box;
    sphere.operation = AgentOperation::CreateSphere;
    sphere.parameters["radius"] = "1.0";
    box.operation = AgentOperation::CreateBox;
    box.parameters["width"] = "1.0";
    box.parameters["height"] = "1.0";
    box.parameters["depth"] = "1.0";

    auto resp = loop.executeIntent("create a shape", {sphere, box});
    EXPECT_TRUE(resp.success);
    EXPECT_GT(loop.status().candidatesEvaluated, 0u);
}

TEST(AgentLoop, ExecuteCommandDirectly)
{
    AgentLoop loop;
    AgentCommand cmd;
    cmd.operation = AgentOperation::CreateSphere;
    cmd.parameters["radius"] = "0.5";
    auto resp = loop.executeCommand(cmd);
    EXPECT_TRUE(resp.success);
}

TEST(AgentLoop, PlannerEnabledEvaluatesCandidates)
{
    AgentLoopConfig config;
    config.enablePlanner = true;
    AgentLoop loop(config);

    AgentCommand a, b;
    a.operation = AgentOperation::CreateBox;
    a.parameters["width"] = "1.0";
    a.parameters["height"] = "1.0";
    a.parameters["depth"] = "1.0";
    b.operation = AgentOperation::CreateCylinder;
    b.parameters["radius"] = "0.5";
    b.parameters["height"] = "2.0";

    auto resp = loop.executeIntent("build something", {a, b});
    EXPECT_TRUE(resp.success);
    EXPECT_GE(loop.status().candidatesEvaluated, 2u);
}

TEST(AgentLoop, RecorderGeneratesCorpus)
{
    AgentLoopConfig config;
    config.enableRecorder = true;
    AgentLoop loop(config);

    AgentCommand cmd;
    cmd.operation = AgentOperation::CreateBox;
    cmd.parameters["width"] = "1.0";
    cmd.parameters["height"] = "1.0";
    cmd.parameters["depth"] = "1.0";

    loop.executeIntent("box", {cmd});

    auto corpus = loop.exportTrainingCorpus();
    EXPECT_FALSE(corpus.empty());
}

TEST(AgentLoop, EmptyCandidatesReturnsFailure)
{
    AgentLoop loop;
    auto resp = loop.executeIntent("test", {});
    EXPECT_FALSE(resp.success);
}

TEST(AgentLoop, StatusTracksExecution)
{
    AgentLoop loop;
    AgentCommand cmd;
    cmd.operation = AgentOperation::CreateBox;
    cmd.parameters["width"] = "1.0";
    cmd.parameters["height"] = "1.0";
    cmd.parameters["depth"] = "1.0";

    loop.executeIntent("status_test", {cmd});
    EXPECT_TRUE(loop.status().sessionValid);
    EXPECT_GT(loop.status().stepsExecuted, 0u);
}

TEST(AgentLoop, AccessSubcomponents)
{
    AgentLoop loop;
    loop.planner().scoreGeometry(loop.session().document());
    (void)loop.session().document().history().featureCount();
    EXPECT_EQ(loop.recorder().episodeCount(), 0u);
}

TEST(AgentLoop, ConfigControlsBehavior)
{
    AgentLoopConfig config;
    config.enableRecorder = false;
    config.enablePlanner = false;
    AgentLoop loop(config);

    AgentCommand cmd;
    cmd.operation = AgentOperation::CreateBox;
    cmd.parameters["width"] = "1.0";
    cmd.parameters["height"] = "1.0";
    cmd.parameters["depth"] = "1.0";

    loop.executeIntent("no_record", {cmd});
    EXPECT_EQ(loop.recorder().episodeCount(), 0u);
}
