#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "nexus/utility/logging/logger.h"
#include "nexus/utility/logging/log_sinks.h"

#include <fstream>
#include <cstdio>
#include <filesystem>

using namespace nexus::utility::logging;

// ── Test Sink ───────────────────────────────────────────────────────────────
class TestSink : public LogSink {
public:
    using Callback = std::function<void(const LogRecord&)>;
    explicit TestSink(Callback cb) : callback_(std::move(cb)) {}

    void write(const LogRecord& record) override { if (callback_) callback_(record); }
    void flush() override {}

private:
    Callback callback_;
};

class LoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        Logger::instance().clearSinks();
        Logger::instance().setLogLevel(LogLevel::Trace);
        Logger::instance().setTimestamps(false);
        Logger::instance().setConsoleOutput(false);
        test_file_ = std::filesystem::temp_directory_path() / "nexus_logger_test.log";
        std::filesystem::remove(test_file_);
    }

    void TearDown() override {
        Logger::instance().clearSinks();
        std::filesystem::remove(test_file_);
    }

    std::filesystem::path test_file_;
};

TEST_F(LoggerTest, SingletonReturnsSameInstance) {
    auto& a = Logger::instance();
    auto& b = Logger::instance();
    EXPECT_EQ(&a, &b);
}

TEST_F(LoggerTest, LogLevelFiltering) {
    auto& logger = Logger::instance();
    logger.setLogLevel(LogLevel::Warning);

    int call_count = 0;
    auto test_sink = std::make_shared<TestSink>([&](const LogRecord&) { ++call_count; });
    logger.addSink(test_sink);

    logger.log(LogLevel::Debug, "debug message");
    EXPECT_EQ(call_count, 0);

    logger.log(LogLevel::Warning, "warning message");
    EXPECT_EQ(call_count, 1);

    logger.log(LogLevel::Critical, "critical message");
    EXPECT_EQ(call_count, 2);
}

TEST_F(LoggerTest, AddAndClearSinks) {
    auto& logger = Logger::instance();
    EXPECT_NO_THROW(logger.addConsoleSink());
    EXPECT_NO_THROW(logger.addFileSink(test_file_.string()));
    EXPECT_NO_THROW(logger.addRotatingFileSink(test_file_.string(), 1024, 3));

    logger.log(LogLevel::Info, "test message");
    EXPECT_NO_THROW(logger.flush());

    logger.clearSinks();
    EXPECT_NO_THROW(logger.flush());
}

TEST_F(LoggerTest, FileSinkWritesToFile) {
    auto& logger = Logger::instance();
    logger.clearSinks();
    logger.addFileSink(test_file_.string());
    logger.setTimestamps(false);

    logger.log(LogLevel::Info, "hello file");
    logger.flush();

    std::ifstream f(test_file_);
    std::string content;
    std::getline(f, content);
    EXPECT_THAT(content, ::testing::HasSubstr("hello file"));
}

TEST_F(LoggerTest, ConsoleOutputToggle) {
    auto& logger = Logger::instance();
    logger.clearSinks();

    logger.setConsoleOutput(true);
    EXPECT_TRUE(logger.isConsoleOutput());

    logger.setConsoleOutput(false);
    EXPECT_FALSE(logger.isConsoleOutput());
}

TEST_F(LoggerTest, TimestampToggle) {
    auto& logger = Logger::instance();
    logger.setTimestamps(true);
    EXPECT_TRUE(logger.isTimestamps());

    logger.setTimestamps(false);
    EXPECT_FALSE(logger.isTimestamps());
}

TEST_F(LoggerTest, LogMacrosDoNotThrow) {
    EXPECT_NO_THROW(NEXUS_LOG_TRACE("trace"));
    EXPECT_NO_THROW(NEXUS_LOG_DEBUG("debug"));
    EXPECT_NO_THROW(NEXUS_LOG_INFO("info"));
    EXPECT_NO_THROW(NEXUS_LOG_WARN("warn"));
    EXPECT_NO_THROW(NEXUS_LOG_ERROR("error"));
    EXPECT_NO_THROW(NEXUS_LOG_CRIT("critical"));
}

TEST_F(LoggerTest, FormatMacrosDoNotThrow) {
    EXPECT_NO_THROW(NEXUS_LOG_INFO_FMT("formatted: " << 42));
    EXPECT_NO_THROW(NEXUS_LOG_ERROR_FMT("error: " << "boom"));
}
