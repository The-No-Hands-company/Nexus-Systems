// ─────────────────────────────────────────────────────────────────────────────
//  Test: Agent Planner — candidate evaluation and heuristic scoring
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/agent/AgentPlanner.h>
#include <nexus/agent/AgentSession.h>
#include <nexus/cad/CadCommand.h>

#include <gtest/gtest.h>

using namespace nexus::agent;

namespace {

void populateSession(AgentSession& session) {
    AgentCommand box;
    box.operation = AgentOperation::CreateBox;
    box.parameters["width"] = "2.0";
    box.parameters["height"] = "1.0";
    box.parameters["depth"] = "3.0";
    session.execute(box);
}

} // namespace

TEST(AgentPlanner, EvaluateSingleCandidate)
{
    AgentSession session;
    populateSession(session);

    AgentPlanner planner;

    AgentCommand createCmd;
    createCmd.operation = AgentOperation::CreateSphere;
    createCmd.parameters["radius"] = "0.5";
    createCmd.parameters["segments"] = "16";

    std::vector<AgentCommand> candidates = {createCmd};
    auto result = planner.evaluateCandidates(session, candidates);
    EXPECT_TRUE(result.valid);
    EXPECT_EQ(result.allCandidates.size(), 1u);
    EXPECT_GT(result.bestCandidate.score, 0.0f);
}

TEST(AgentPlanner, EvaluateMultipleCandidates)
{
    AgentSession session;
    populateSession(session);
    AgentPlanner planner;

    AgentCommand sphere, cylinder, cone;
    sphere.operation = AgentOperation::CreateSphere;
    sphere.parameters["radius"] = "0.5";
    cylinder.operation = AgentOperation::CreateCylinder;
    cylinder.parameters["radius"] = "0.5";
    cylinder.parameters["height"] = "1.0";
    cone.operation = AgentOperation::CreateCone;
    cone.parameters["radius"] = "0.5";
    cone.parameters["height"] = "1.0";

    std::vector<AgentCommand> candidates = {sphere, cylinder, cone};
    auto result = planner.evaluateCandidates(session, candidates);
    EXPECT_TRUE(result.valid);
    EXPECT_EQ(result.allCandidates.size(), 3u);
    EXPECT_FALSE(result.planExplanation.empty());
}

TEST(AgentPlanner, ScoreGeometryOnPopulated)
{
    AgentSession session;
    populateSession(session);
    AgentPlanner planner;
    float score = planner.scoreGeometry(session.document());
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 2.0f);
}

TEST(AgentPlanner, ScoreTopologyCleanliness)
{
    AgentSession session;
    populateSession(session);
    AgentPlanner planner;
    float score = planner.scoreTopologyCleanliness(session.document());
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 1.5f);
}

TEST(AgentPlanner, EvaluateEmptyCandidates)
{
    AgentSession session;
    populateSession(session);
    AgentPlanner planner;
    auto result = planner.evaluateCandidates(session, {});
    EXPECT_TRUE(result.valid);
    EXPECT_TRUE(result.allCandidates.empty());
}

TEST(AgentPlanner, FailedCommandScoresNegative)
{
    AgentSession session;
    populateSession(session);
    AgentPlanner planner;

    AgentCommand invalidCmd;
    invalidCmd.operation = AgentOperation::AddConstraint;

    std::vector<AgentCommand> candidates = {invalidCmd};
    auto result = planner.evaluateCandidates(session, candidates);
    EXPECT_TRUE(result.valid);
    EXPECT_EQ(result.allCandidates.size(), 1u);
    EXPECT_LT(result.allCandidates[0].score, 0.0f);
}

TEST(AgentPlanner, DefaultContext)
{
    AgentPlanner planner;
    auto& ctx = planner.defaultContext();
    EXPECT_GT(ctx.topologyWeight, 0.0f);
    EXPECT_GT(ctx.validityWeight, 0.0f);
    EXPECT_GT(ctx.efficiencyWeight, 0.0f);
}

TEST(AgentPlanner, SetDefaultContext)
{
    AgentPlanner planner;
    PlanningContext ctx;
    ctx.topologyWeight = 0.5f;
    ctx.validityWeight = 0.3f;
    ctx.efficiencyWeight = 0.2f;
    planner.setDefaultContext(ctx);
    EXPECT_FLOAT_EQ(planner.defaultContext().topologyWeight, 0.5f);
}
