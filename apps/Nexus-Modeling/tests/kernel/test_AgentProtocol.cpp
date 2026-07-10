// ─────────────────────────────────────────────────────────────────────────────
//  Test: Agent Protocol — command parsing, serialization, round-trip
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/agent/AgentProtocol.h>

#include <gtest/gtest.h>

using namespace nexus::agent;

TEST(AgentProtocol, ParseOperationByName)
{
    EXPECT_EQ(parseOperation("noop"), AgentOperation::NoOp);
    EXPECT_EQ(parseOperation("create_box"), AgentOperation::CreateBox);
    EXPECT_EQ(parseOperation("create_cylinder"), AgentOperation::CreateCylinder);
    EXPECT_EQ(parseOperation("create_sphere"), AgentOperation::CreateSphere);
    EXPECT_EQ(parseOperation("create_cone"), AgentOperation::CreateCone);
    EXPECT_EQ(parseOperation("extrude"), AgentOperation::Extrude);
    EXPECT_EQ(parseOperation("revolve"), AgentOperation::Revolve);
    EXPECT_EQ(parseOperation("fillet"), AgentOperation::Fillet);
    EXPECT_EQ(parseOperation("chamfer"), AgentOperation::Chamfer);
    EXPECT_EQ(parseOperation("boolean_union"), AgentOperation::BooleanUnion);
    EXPECT_EQ(parseOperation("boolean_diff"), AgentOperation::BooleanDifference);
    EXPECT_EQ(parseOperation("boolean_intersect"), AgentOperation::BooleanIntersection);
    EXPECT_EQ(parseOperation("shell"), AgentOperation::Shell);
    EXPECT_EQ(parseOperation("draft"), AgentOperation::Draft);
    EXPECT_EQ(parseOperation("pattern_linear"), AgentOperation::PatternLinear);
    EXPECT_EQ(parseOperation("pattern_circular"), AgentOperation::PatternCircular);
    EXPECT_EQ(parseOperation("mirror"), AgentOperation::Mirror);
    EXPECT_EQ(parseOperation("transform"), AgentOperation::Transform);
    EXPECT_EQ(parseOperation("delete"), AgentOperation::Delete);
    EXPECT_EQ(parseOperation("query"), AgentOperation::Query);
    EXPECT_EQ(parseOperation("add_constraint"), AgentOperation::AddConstraint);
    EXPECT_EQ(parseOperation("solve_constraints"), AgentOperation::SolveConstraints);
    EXPECT_EQ(parseOperation("select"), AgentOperation::Select);
    EXPECT_EQ(parseOperation("deselect"), AgentOperation::Deselect);
    EXPECT_EQ(parseOperation("undo"), AgentOperation::Undo);
    EXPECT_EQ(parseOperation("redo"), AgentOperation::Redo);
}

TEST(AgentProtocol, UnknownOperationReturnsNoOp)
{
    EXPECT_EQ(parseOperation("nonexistent_op"), AgentOperation::NoOp);
    EXPECT_EQ(parseOperation(""), AgentOperation::NoOp);
}

TEST(AgentProtocol, OperationNameRoundTrip)
{
    for (int i = 0; i < static_cast<int>(AgentOperation::Count); ++i) {
        auto op = static_cast<AgentOperation>(i);
        auto name = operationName(op);
        EXPECT_NE(name, nullptr);
        EXPECT_STREQ(name, operationName(static_cast<AgentOperation>(i)));
    }
}

TEST(AgentProtocol, ParseCommandBasic)
{
    auto cmd = parseCommand("create_box\nwidth=2.0\nheight=1.5\ndepth=3.0");
    EXPECT_EQ(cmd.operation, AgentOperation::CreateBox);
    EXPECT_EQ(cmd.parameters.at("width"), "2.0");
    EXPECT_EQ(cmd.parameters.at("height"), "1.5");
    EXPECT_EQ(cmd.parameters.at("depth"), "3.0");
}

TEST(AgentProtocol, ParseEmptyCommand)
{
    auto cmd = parseCommand("");
    EXPECT_EQ(cmd.operation, AgentOperation::NoOp);
    EXPECT_TRUE(cmd.parameters.empty());
}

TEST(AgentProtocol, ParseSingleLineCommand)
{
    auto cmd = parseCommand("undo");
    EXPECT_EQ(cmd.operation, AgentOperation::Undo);
    EXPECT_TRUE(cmd.parameters.empty());
}

TEST(AgentProtocol, SerializeCommandRoundTrip)
{
    AgentCommand cmd;
    cmd.operation = AgentOperation::CreateSphere;
    cmd.parameters["radius"] = "0.5";
    cmd.parameters["segments"] = "16";
    auto text = serializeCommand(cmd);
    auto parsed = parseCommand(text);
    EXPECT_EQ(parsed.operation, cmd.operation);
    EXPECT_EQ(parsed.parameters.at("radius"), "0.5");
    EXPECT_EQ(parsed.parameters.at("segments"), "16");
}

TEST(AgentProtocol, SerializeResponse)
{
    AgentResponse resp;
    resp.success = true;
    AgentStateReport report;
    report.valid = true;
    TopologyChange change;
    change.type = TopologyChange::ChangeType::Created;
    change.elementType = "Face";
    change.indices = {0, 1, 2};
    report.topologyChanges.push_back(change);
    resp.stateReports.push_back(std::move(report));
    auto text = serializeResponse(resp);
    EXPECT_TRUE(text.find("success=true") != std::string::npos);
    EXPECT_TRUE(text.find("report_count=1") != std::string::npos);
}

TEST(AgentProtocol, ParseResponse)
{
    auto text = "success=true\nerror=some error";
    auto resp = parseResponse(text);
    EXPECT_TRUE(resp.success);
    EXPECT_EQ(resp.errorMessage, "some error");
}

TEST(AgentProtocol, ParseResponseFailure)
{
    auto text = "success=false";
    auto resp = parseResponse(text);
    EXPECT_FALSE(resp.success);
}
