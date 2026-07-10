// ─────────────────────────────────────────────────────────────────────────────
//  Test: Agent Training Recorder — episode recording and corpus export
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/agent/AgentTrainingRecorder.h>

#include <gtest/gtest.h>

using namespace nexus::agent;

TEST(AgentTrainingRecorder, BeginEpisode)
{
    AgentTrainingRecorder recorder;
    recorder.beginEpisode("test_episode");
    EXPECT_EQ(recorder.episodeCount(), 1u);
}

TEST(AgentTrainingRecorder, RecordStep)
{
    AgentTrainingRecorder recorder;
    recorder.beginEpisode("test");
    AgentCommand cmd;
    cmd.operation = AgentOperation::CreateBox;
    AgentResponse resp;
    resp.success = true;
    recorder.recordStep(cmd, resp);
    recorder.endEpisode();
    EXPECT_EQ(recorder.totalSteps(), 1u);
}

TEST(AgentTrainingRecorder, MultipleEpisodes)
{
    AgentTrainingRecorder recorder;
    recorder.beginEpisode("ep1");
    AgentCommand cmd;
    cmd.operation = AgentOperation::CreateBox;
    AgentResponse resp;
    resp.success = true;
    recorder.recordStep(cmd, resp);
    recorder.endEpisode();

    recorder.beginEpisode("ep2");
    recorder.recordStep(cmd, resp);
    recorder.recordStep(cmd, resp);
    recorder.endEpisode();

    EXPECT_EQ(recorder.episodeCount(), 2u);
    EXPECT_EQ(recorder.totalSteps(), 3u);
}

TEST(AgentTrainingRecorder, ExportCorpus)
{
    AgentTrainingRecorder recorder;
    recorder.beginEpisode("ep1");
    AgentCommand cmd;
    cmd.operation = AgentOperation::CreateBox;
    cmd.parameters["width"] = "2.0";
    AgentResponse resp;
    resp.success = true;
    AgentStateReport report;
    report.messages.push_back("box created");
    resp.stateReports.push_back(report);
    recorder.recordStep(cmd, resp);
    recorder.endEpisode(1.0f);

    CorpusExportOptions opts;
    opts.prettyPrint = true;
    opts.includeScores = true;

    auto corpus = recorder.exportCorpus(opts);
    EXPECT_FALSE(corpus.empty());
    EXPECT_NE(corpus.find("ep1"), std::string::npos);
}

TEST(AgentTrainingRecorder, ExportEpisodeCompact)
{
    AgentTrainingRecorder recorder;
    recorder.beginEpisode("compact_test");
    AgentCommand cmd;
    cmd.operation = AgentOperation::CreateCylinder;
    AgentResponse resp;
    resp.success = true;
    recorder.recordStep(cmd, resp);
    recorder.endEpisode();

    CorpusExportOptions opts;
    opts.prettyPrint = false;

    auto text = recorder.exportEpisode(0, opts);
    EXPECT_FALSE(text.empty());
    EXPECT_NE(text.find("COMMAND"), std::string::npos);
}

TEST(AgentTrainingRecorder, Clear)
{
    AgentTrainingRecorder recorder;
    recorder.beginEpisode("ep");
    AgentCommand cmd;
    cmd.operation = AgentOperation::CreateBox;
    AgentResponse resp;
    resp.success = true;
    recorder.recordStep(cmd, resp);
    recorder.endEpisode();
    EXPECT_EQ(recorder.episodeCount(), 1u);

    recorder.clear();
    EXPECT_EQ(recorder.episodeCount(), 0u);
    EXPECT_EQ(recorder.totalSteps(), 0u);
}

TEST(AgentTrainingRecorder, TagEpisode)
{
    AgentTrainingRecorder recorder;
    recorder.beginEpisode("tagged");
    recorder.tagEpisode("modeling");
    recorder.tagEpisode("box");
    AgentCommand cmd;
    cmd.operation = AgentOperation::CreateBox;
    AgentResponse resp;
    resp.success = true;
    recorder.recordStep(cmd, resp);
    recorder.endEpisode();

    CorpusExportOptions opts;
    opts.prettyPrint = true;
    auto corpus = recorder.exportCorpus(opts);
    EXPECT_NE(corpus.find("modeling"), std::string::npos);
    EXPECT_NE(corpus.find("box"), std::string::npos);
}
