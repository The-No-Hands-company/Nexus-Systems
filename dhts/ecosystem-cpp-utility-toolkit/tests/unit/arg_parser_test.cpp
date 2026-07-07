#include <gtest/gtest.h>
#include <string>
#include <cstring>

#include "nexus/utility/cli/arg_parser.h"

using namespace nexus::utility::cli;

class ArgParserTest : public ::testing::Test {
protected:
    void parseArgs(ArgParser& parser, std::vector<std::string> args) {
        std::vector<char*> argv;
        std::string prog = "test";
        argv.push_back(prog.data());
        for (auto& a : args) {
            argv.push_back(a.data());
        }
        parser.parse(static_cast<int>(argv.size()), argv.data());
    }
};

TEST_F(ArgParserTest, FlagSet) {
    ArgParser parser("test");
    parser.addFlag("-v", "--verbose", "Verbose output");

    parseArgs(parser, {"--verbose"});
    EXPECT_TRUE(parser.get<bool>("--verbose"));
    EXPECT_TRUE(parser.isSet("--verbose"));
}

TEST_F(ArgParserTest, FlagNotSet) {
    ArgParser parser("test");
    parser.addFlag("-v", "--verbose", "Verbose output");

    parseArgs(parser, {});
    EXPECT_FALSE(parser.get<bool>("--verbose"));
}

TEST_F(ArgParserTest, ShortFlag) {
    ArgParser parser("test");
    parser.addFlag("-v", "--verbose", "Verbose");

    parseArgs(parser, {"-v"});
    EXPECT_TRUE(parser.get<bool>("--verbose"));
    EXPECT_TRUE(parser.isSet("-v"));
}

TEST_F(ArgParserTest, IntOption) {
    ArgParser parser("test");
    parser.addOption<int>("-p", "--port", "Port number", 8080);

    parseArgs(parser, {"--port", "3000"});
    EXPECT_EQ(parser.get<int>("--port"), 3000);
}

TEST_F(ArgParserTest, IntOptionDefault) {
    ArgParser parser("test");
    parser.addOption<int>("-p", "--port", "Port", 8080);

    parseArgs(parser, {});
    EXPECT_EQ(parser.get<int>("--port"), 8080);
}

TEST_F(ArgParserTest, StringOption) {
    ArgParser parser("test");
    parser.addOption<std::string>("-n", "--name", "Name", "default");

    parseArgs(parser, {"--name", "Alice"});
    EXPECT_EQ(parser.get<std::string>("--name"), "Alice");
}

TEST_F(ArgParserTest, EqualsSyntax) {
    ArgParser parser("test");
    parser.addOption<std::string>("-n", "--name", "Name", "default");

    parseArgs(parser, {"--name=Bob"});
    EXPECT_EQ(parser.get<std::string>("--name"), "Bob");
}

TEST_F(ArgParserTest, PositionalArgs) {
    ArgParser parser("test");
    parser.addFlag("-v", "--verbose", "Verbose");

    parseArgs(parser, {"file1.txt", "file2.txt", "--verbose"});
    auto pos = parser.getPositional();
    ASSERT_EQ(pos.size(), 2);
    EXPECT_EQ(pos[0], "file1.txt");
    EXPECT_EQ(pos[1], "file2.txt");
    EXPECT_TRUE(parser.get<bool>("--verbose"));
}

TEST_F(ArgParserTest, UnknownOptionThrows) {
    ArgParser parser("test");
    EXPECT_THROW(parseArgs(parser, {"--unknown"}), std::runtime_error);
}

TEST_F(ArgParserTest, MissingValueThrows) {
    ArgParser parser("test");
    parser.addOption<int>("-p", "--port", "Port", 8080);

    EXPECT_THROW(parseArgs(parser, {"--port"}), std::runtime_error);
}

TEST_F(ArgParserTest, UnknownArgNameThrows) {
    ArgParser parser("test");
    EXPECT_THROW(parser.get<int>("--nonexistent"), std::runtime_error);
}

TEST_F(ArgParserTest, IsSetFalse) {
    ArgParser parser("test");
    parser.addFlag("-v", "--verbose", "Verbose");
    parser.addOption<int>("-p", "--port", "Port", 8080);

    parseArgs(parser, {});
    EXPECT_FALSE(parser.isSet("--verbose"));
    EXPECT_FALSE(parser.isSet("--port"));
}

TEST_F(ArgParserTest, MultipleFlags) {
    ArgParser parser("test");
    parser.addFlag("-v", "--verbose", "Verbose");
    parser.addFlag("-q", "--quiet", "Quiet mode");

    parseArgs(parser, {"--verbose", "--quiet"});
    EXPECT_TRUE(parser.get<bool>("--verbose"));
    EXPECT_TRUE(parser.get<bool>("--quiet"));
}

TEST_F(ArgParserTest, MixedArgs) {
    ArgParser parser("test");
    parser.addFlag("-d", "--debug", "Debug mode");
    parser.addOption<int>("-p", "--port", "Port", 3000);
    parser.addOption<std::string>("-o", "--output", "Output file", "out.txt");

    parseArgs(parser, {"--debug", "--port", "5000", "--output=result.log", "input.dat"});
    EXPECT_TRUE(parser.get<bool>("--debug"));
    EXPECT_EQ(parser.get<int>("--port"), 5000);
    EXPECT_EQ(parser.get<std::string>("--output"), "result.log");
    auto pos = parser.getPositional();
    ASSERT_EQ(pos.size(), 1);
    EXPECT_EQ(pos[0], "input.dat");
}

TEST_F(ArgParserTest, GetByShortName) {
    ArgParser parser("test");
    parser.addOption<int>("-p", "--port", "Port", 8080);

    parseArgs(parser, {"-p", "3000"});
    EXPECT_EQ(parser.get<int>("-p"), 3000);
}
