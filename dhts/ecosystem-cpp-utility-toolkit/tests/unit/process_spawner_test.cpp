#include <gtest/gtest.h>
#include <string>
#include <chrono>

#include "nexus/utility/system/process_spawner.h"

using namespace nexus::utility::system;

TEST(ProcessSpawnerTest, EchosInput) {
    ProcessSpawner::Options opts;
    opts.args = {"hello"};
    auto result = ProcessSpawner::execute("/bin/echo", opts);
    EXPECT_EQ(result.exit_code, 0);
    EXPECT_FALSE(result.timed_out);
    EXPECT_TRUE(result.stdout_output.find("hello") != std::string::npos);
}

TEST(ProcessSpawnerTest, ShellCommand) {
    auto result = ProcessSpawner::executeShell("echo test123");
    EXPECT_EQ(result.exit_code, 0);
    EXPECT_TRUE(result.stdout_output.find("test123") != std::string::npos);
}

TEST(ProcessSpawnerTest, StdoutCapture) {
    ProcessSpawner::Options opts;
    opts.args = {"-c", "echo stdout; echo stderr >&2"};
    auto result = ProcessSpawner::execute("/bin/sh", opts);
    EXPECT_EQ(result.exit_code, 0);
    EXPECT_TRUE(result.stdout_output.find("stdout") != std::string::npos);
    EXPECT_TRUE(result.stderr_output.find("stderr") != std::string::npos);
}

TEST(ProcessSpawnerTest, StderrMerge) {
    ProcessSpawner::Options opts;
    opts.args = {"-c", "echo to_stdout; echo to_stderr >&2"};
    opts.merge_stderr = true;

    auto result = ProcessSpawner::execute("/bin/sh", opts);
    EXPECT_EQ(result.exit_code, 0);
    EXPECT_TRUE(result.stdout_output.find("to_stdout") != std::string::npos);
    EXPECT_TRUE(result.stdout_output.find("to_stderr") != std::string::npos);
    EXPECT_TRUE(result.stderr_output.empty());
}

TEST(ProcessSpawnerTest, StdinInput) {
    ProcessSpawner::Options opts;
    opts.stdin_input = "hello stdin";
    opts.args = {};

    auto result = ProcessSpawner::execute("/bin/cat", opts);
    EXPECT_EQ(result.exit_code, 0);
    EXPECT_EQ(result.stdout_output, "hello stdin");
}

TEST(ProcessSpawnerTest, ExitCode) {
    auto result = ProcessSpawner::executeShell("exit 42");
    EXPECT_EQ(result.exit_code, 42);
}

TEST(ProcessSpawnerTest, NonExistentCommand) {
    ProcessSpawner::Options opts;
    auto result = ProcessSpawner::execute("/nonexistent/command", opts);
    EXPECT_EQ(result.exit_code, 127);
}

TEST(ProcessSpawnerTest, Timeout) {
    ProcessSpawner::Options opts;
    opts.args = {"-c", "sleep 10"};
    opts.timeout = std::chrono::milliseconds(100);

    auto result = ProcessSpawner::execute("/bin/sh", opts);
    EXPECT_TRUE(result.timed_out);
}

TEST(ProcessSpawnerTest, LargeOutput) {
    // Generate a large output to test that pipes don't deadlock
    auto result = ProcessSpawner::executeShell(
        "for i in $(seq 1 1000); do echo $i; done");
    EXPECT_EQ(result.exit_code, 0);
    EXPECT_GT(result.stdout_output.size(), 1000);
}
