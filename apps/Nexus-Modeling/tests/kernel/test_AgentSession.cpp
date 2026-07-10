// ─────────────────────────────────────────────────────────────────────────────
//  Test: Agent Session — headless command execution
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/agent/AgentSession.h>
#include <nexus/agent/AgentObserver.h>

#include <gtest/gtest.h>

using namespace nexus::agent;

namespace {

class TestListener : public AgentMutationListener {
public:
    void onTopologyChanged(const AgentStateReport&) override { topologyCalls++; }
    void onCommandExecuted(const AgentCommand&, const AgentResponse&) override { commandCalls++; }
    int topologyCalls = 0;
    int commandCalls = 0;
};

} // namespace

TEST(AgentSession, CreateBox)
{
    AgentSession session;
    AgentCommand cmd;
    cmd.operation = AgentOperation::CreateBox;
    cmd.parameters["width"] = "2.0";
    cmd.parameters["height"] = "1.0";
    cmd.parameters["depth"] = "3.0";
    auto resp = session.execute(cmd);
    EXPECT_TRUE(resp.success);
    EXPECT_GE(session.document().history().featureCount(), 1u);
}

TEST(AgentSession, CreateCylinder)
{
    AgentSession session;
    AgentCommand cmd;
    cmd.operation = AgentOperation::CreateCylinder;
    cmd.parameters["radius"] = "0.5";
    cmd.parameters["height"] = "2.0";
    cmd.parameters["segments"] = "16";
    auto resp = session.execute(cmd);
    EXPECT_TRUE(resp.success);
    EXPECT_GE(session.document().history().featureCount(), 1u);
}

TEST(AgentSession, CreateSphere)
{
    AgentSession session;
    AgentCommand cmd;
    cmd.operation = AgentOperation::CreateSphere;
    cmd.parameters["radius"] = "1.0";
    cmd.parameters["segments"] = "16";
    auto resp = session.execute(cmd);
    EXPECT_TRUE(resp.success);
    auto* node = session.document().history().node(1);
    ASSERT_NE(node, nullptr);
    ASSERT_TRUE(node->mesh.has_value());
    EXPECT_GT(node->mesh->attributes().vertexCount(), 0u);
    EXPECT_GT(node->mesh->topology().faceCount(), 0u);
}

TEST(AgentSession, CreateCone)
{
    AgentSession session;
    AgentCommand cmd;
    cmd.operation = AgentOperation::CreateCone;
    cmd.parameters["radius"] = "1.0";
    cmd.parameters["height"] = "2.0";
    cmd.parameters["segments"] = "16";
    auto resp = session.execute(cmd);
    EXPECT_TRUE(resp.success);
}

TEST(AgentSession, DeleteFeature)
{
    AgentSession session;
    {
        AgentCommand cmd;
        cmd.operation = AgentOperation::CreateBox;
        cmd.parameters["width"] = "1.0";
        cmd.parameters["height"] = "1.0";
        cmd.parameters["depth"] = "1.0";
        (void)session.execute(cmd);
    }
    ASSERT_GE(session.document().history().featureCount(), 1u);
    AgentCommand del;
    del.operation = AgentOperation::Delete;
    del.parameters["feature_id"] = "1";
    auto resp = session.execute(del);
    EXPECT_TRUE(resp.success);
}

TEST(AgentSession, UndoRedo)
{
    AgentSession session;
    {
        AgentCommand cmd;
        cmd.operation = AgentOperation::CreateBox;
        cmd.parameters["width"] = "1.0";
        cmd.parameters["height"] = "1.0";
        cmd.parameters["depth"] = "1.0";
        (void)session.execute(cmd);
    }
    auto undoResp = session.undo();
    EXPECT_TRUE(undoResp.success);
    auto redoResp = session.redo();
    EXPECT_TRUE(redoResp.success);
}

TEST(AgentSession, QueryReportsFeatures)
{
    AgentSession session;
    {
        AgentCommand cmd;
        cmd.operation = AgentOperation::CreateBox;
        cmd.parameters["width"] = "1.0";
        cmd.parameters["height"] = "1.0";
        cmd.parameters["depth"] = "1.0";
        session.execute(cmd);
    }
    AgentCommand q;
    q.operation = AgentOperation::Query;
    auto resp = session.execute(q);
    EXPECT_TRUE(resp.success);
    EXPECT_GE(resp.stateReports.size(), 1u);
}

TEST(AgentSession, SerializeDeserializeRoundTrip)
{
    AgentSession session;
    {
        AgentCommand cmd;
        cmd.operation = AgentOperation::CreateBox;
        cmd.parameters["width"] = "1.0";
        cmd.parameters["height"] = "2.0";
        cmd.parameters["depth"] = "3.0";
        session.execute(cmd);
    }
    auto data = session.serializeState();
    EXPECT_FALSE(data.empty());
    AgentSession session2;
    EXPECT_TRUE(session2.deserializeState(data));
    EXPECT_EQ(session2.document().history().featureCount(),
              session.document().history().featureCount());
}

TEST(AgentSession, ValidateCommand)
{
    AgentSession session;
    auto* node = session.document().history().node(999);
    EXPECT_EQ(node, nullptr);
}

TEST(AgentSession, ObserverNotification)
{
    AgentSession session;
    TestListener listener;
    session.notifier().addListener(&listener);
    AgentCommand cmd;
    cmd.operation = AgentOperation::CreateBox;
    cmd.parameters["width"] = "1.0";
    cmd.parameters["height"] = "1.0";
    cmd.parameters["depth"] = "1.0";
    session.execute(cmd);
    EXPECT_GT(listener.commandCalls, 0);
}
