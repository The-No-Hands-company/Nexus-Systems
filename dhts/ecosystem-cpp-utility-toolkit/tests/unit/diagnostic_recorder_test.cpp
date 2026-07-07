#include <gtest/gtest.h>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>

#include "nexus/utility/meta/diagnostic_recorder.h"

using namespace nexus::utility::meta;

// ── Example diagnostic using the CRTP base ─────────────────────────────────
class TestDiagnostic : public DiagnosticRecorder<TestDiagnostic> {
public:
    void recordMetric(const std::string& name, double value) {
        recordEvent(name + " = " + std::to_string(value), "info");
    }
    void recordFailure(const std::string& reason) {
        recordError(reason);
    }
};

// ── Tests ──────────────────────────────────────────────────────────────────

TEST(DiagnosticRecorderTest, Singleton) {
    auto& a = TestDiagnostic::instance();
    auto& b = TestDiagnostic::instance();
    EXPECT_EQ(&a, &b);
}

TEST(DiagnosticRecorderTest, EmitAndRetrieve) {
    auto& diag = TestDiagnostic::instance();
    diag.initialize();

    diag.recordMetric("cpu", 75.5);
    diag.recordFailure("disk full");

    auto history = diag.getHistory();
    EXPECT_EQ(history.size(), 2);
    EXPECT_EQ(diag.getCount(), 2);
    EXPECT_TRUE(history[0].message.find("cpu") != std::string::npos);
    EXPECT_EQ(history[1].severity, "error");

    diag.shutdown();
}

TEST(DiagnosticRecorderTest, EnableDisable) {
    auto& diag = TestDiagnostic::instance();
    diag.initialize();

    diag.disable();
    diag.recordMetric("test", 1.0);
    EXPECT_EQ(diag.getCount(), 0);

    diag.enable();
    diag.recordMetric("test", 1.0);
    EXPECT_EQ(diag.getCount(), 1);

    diag.shutdown();
}

TEST(DiagnosticRecorderTest, MaxHistory) {
    auto& diag = TestDiagnostic::instance();
    diag.initialize();
    diag.setMaxHistory(3);

    for (int i = 0; i < 10; ++i) {
        diag.recordMetric("m", i);
    }

    EXPECT_EQ(diag.getCount(), 3);
    auto recent = diag.getRecent(2);
    EXPECT_EQ(recent.size(), 2);
    // Should have the last entries
    EXPECT_TRUE(recent[1].message.find("9.0") != std::string::npos);

    diag.shutdown();
}

TEST(DiagnosticRecorderTest, Clear) {
    auto& diag = TestDiagnostic::instance();
    diag.initialize();

    diag.recordMetric("a", 1);
    diag.recordMetric("b", 2);
    EXPECT_EQ(diag.getCount(), 2);

    diag.clear();
    EXPECT_EQ(diag.getCount(), 0);

    diag.shutdown();
}

TEST(DiagnosticRecorderTest, ThreadSafety) {
    auto& diag = TestDiagnostic::instance();
    diag.initialize();
    diag.setMaxHistory(10000);

    std::atomic<int> errors{0};
    constexpr int kThreads = 8;
    constexpr int kEvents = 500;

    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&diag, &errors, t]() {
            for (int i = 0; i < kEvents; ++i) {
                try {
                    diag.recordMetric("t" + std::to_string(t),
                                       static_cast<double>(i));
                } catch (...) {
                    errors.fetch_add(1);
                }
            }
        });
    }
    for (auto& t : threads) t.join();

    EXPECT_EQ(errors.load(), 0);
    EXPECT_GT(diag.getCount(), 0);

    diag.shutdown();
}
