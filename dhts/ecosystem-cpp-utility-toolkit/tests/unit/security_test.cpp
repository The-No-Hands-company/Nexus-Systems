#include <gtest/gtest.h>

#include "nexus/utility/security/sql_injection_detector.h"

using namespace nexus::utility::security;

TEST(SecurityTest, SqlInjectionDetection) {
    SqlInjectionDetector::instance().initialize();

    EXPECT_TRUE(SqlInjectionDetector::instance().scan("SELECT * FROM users WHERE id = 1 OR 'x'='x'").suspicious);
    EXPECT_TRUE(SqlInjectionDetector::instance().scan("DROP TABLE users").suspicious);
    EXPECT_TRUE(SqlInjectionDetector::instance().scan("INSERT INTO users VALUES (1)").suspicious);
    EXPECT_TRUE(SqlInjectionDetector::instance().scan("DELETE FROM users").suspicious);
    EXPECT_TRUE(SqlInjectionDetector::instance().scan("1; DROP TABLE users; --").suspicious);
    EXPECT_TRUE(SqlInjectionDetector::instance().scan("EXEC xp_cmdshell").suspicious);

    EXPECT_FALSE(SqlInjectionDetector::instance().scan("SELECT * FROM users WHERE name = 'John'").suspicious);
    EXPECT_FALSE(SqlInjectionDetector::instance().scan("").suspicious);

    SqlInjectionDetector::instance().shutdown();
}

TEST(SecurityTest, SqlInjectionHistory) {
    SqlInjectionDetector::instance().initialize();
    SqlInjectionDetector::instance().scan("DROP TABLE users");
    SqlInjectionDetector::instance().scan("DROP TABLE orders");

    auto history = SqlInjectionDetector::instance().getHistory();
    EXPECT_GE(history.size(), 2);

    EXPECT_GE(SqlInjectionDetector::instance().getDetectionCount(), 2);

    SqlInjectionDetector::instance().shutdown();
}
