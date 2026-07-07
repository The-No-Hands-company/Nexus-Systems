#include <gtest/gtest.h>

#include "nexus/utility/security/input_validator.h"

using namespace nexus::utility::security;

TEST(InputValidatorTest, IsAlphanumeric) {
    EXPECT_TRUE(InputValidator::isAlphanumeric("abc123"));
    EXPECT_TRUE(InputValidator::isAlphanumeric(""));
    EXPECT_FALSE(InputValidator::isAlphanumeric("abc 123"));
    EXPECT_FALSE(InputValidator::isAlphanumeric("abc!123"));
    EXPECT_FALSE(InputValidator::isAlphanumeric("abc\n123"));
}

TEST(InputValidatorTest, IsPrintable) {
    EXPECT_TRUE(InputValidator::isPrintable("Hello World!"));
    EXPECT_TRUE(InputValidator::isPrintable(""));
    EXPECT_FALSE(InputValidator::isPrintable("Hello\nWorld"));
    EXPECT_FALSE(InputValidator::isPrintable("Hello\tWorld"));
}

TEST(InputValidatorTest, IsValidLength) {
    EXPECT_TRUE(InputValidator::isValidLength("hello", 100));
    EXPECT_TRUE(InputValidator::isValidLength("hello", 5));
    EXPECT_FALSE(InputValidator::isValidLength("", 100));
    EXPECT_FALSE(InputValidator::isValidLength("hello", 3));
}

TEST(InputValidatorTest, IsInRange) {
    EXPECT_TRUE(InputValidator::isInRange(5, 0, 10));
    EXPECT_TRUE(InputValidator::isInRange(0, 0, 10));
    EXPECT_TRUE(InputValidator::isInRange(10, 0, 10));
    EXPECT_FALSE(InputValidator::isInRange(-1, 0, 10));
    EXPECT_FALSE(InputValidator::isInRange(11, 0, 10));
}

TEST(InputValidatorTest, HasNullBytes) {
    std::string with_null("hello", 6); // "hello\0" with embedded null
    with_null[5] = '\0';
    EXPECT_TRUE(InputValidator::hasNullBytes(with_null));
    EXPECT_FALSE(InputValidator::hasNullBytes("hello world"));
    EXPECT_FALSE(InputValidator::hasNullBytes(""));
}

TEST(InputValidatorTest, RemoveNullBytes) {
    std::string input("he\0ll\0o", 7);
    auto result = InputValidator::removeNullBytes(input);
    EXPECT_EQ(result, "hello");

    auto result2 = InputValidator::removeNullBytes("clean");
    EXPECT_EQ(result2, "clean");
}

TEST(InputValidatorTest, HasPathTraversal) {
    EXPECT_TRUE(InputValidator::hasPathTraversal("../../../etc/passwd"));
    EXPECT_TRUE(InputValidator::hasPathTraversal("~/.ssh"));
    EXPECT_TRUE(InputValidator::hasPathTraversal("$HOME"));
    EXPECT_FALSE(InputValidator::hasPathTraversal("/var/log/app.log"));
}

TEST(InputValidatorTest, IsValidEmail) {
    EXPECT_TRUE(InputValidator::isValidEmail("user@example.com"));
    EXPECT_TRUE(InputValidator::isValidEmail("a@b.co"));
    EXPECT_FALSE(InputValidator::isValidEmail("not-an-email"));
    EXPECT_FALSE(InputValidator::isValidEmail("@example.com"));
    EXPECT_FALSE(InputValidator::isValidEmail("user@"));
    EXPECT_FALSE(InputValidator::isValidEmail("user@.com"));
    EXPECT_FALSE(InputValidator::isValidEmail(""));
}

TEST(InputValidatorTest, IsValidUrl) {
    EXPECT_TRUE(InputValidator::isValidUrl("https://example.com"));
    EXPECT_TRUE(InputValidator::isValidUrl("http://example.com/path?q=1"));
    EXPECT_TRUE(InputValidator::isValidUrl("ftp://files.example.com"));
    EXPECT_TRUE(InputValidator::isValidUrl("ws://socket.example.com"));
    EXPECT_FALSE(InputValidator::isValidUrl("not-a-url"));
    EXPECT_FALSE(InputValidator::isValidUrl("example.com"));
    EXPECT_FALSE(InputValidator::isValidUrl(""));
}

TEST(InputValidatorTest, IsValidIPv4) {
    EXPECT_TRUE(InputValidator::isValidIPv4("192.168.1.1"));
    EXPECT_TRUE(InputValidator::isValidIPv4("0.0.0.0"));
    EXPECT_TRUE(InputValidator::isValidIPv4("255.255.255.255"));
    EXPECT_TRUE(InputValidator::isValidIPv4("127.0.0.1"));
    EXPECT_FALSE(InputValidator::isValidIPv4("256.1.1.1"));
    EXPECT_FALSE(InputValidator::isValidIPv4("1.2.3"));
    EXPECT_FALSE(InputValidator::isValidIPv4("1.2.3.4.5"));
    EXPECT_FALSE(InputValidator::isValidIPv4("abc.def.ghi.jkl"));
    EXPECT_FALSE(InputValidator::isValidIPv4(""));
}

TEST(InputValidatorTest, SecureZeroMemory) {
    char buffer[16];
    std::memset(buffer, 0xAA, sizeof(buffer));
    secureZeroMemory(buffer, sizeof(buffer));
    for (size_t i = 0; i < sizeof(buffer); ++i) {
        EXPECT_EQ(buffer[i], 0);
    }
}

TEST(InputValidatorTest, SecureZeroString) {
    std::string secret = "super_secret_password_12345";
    secureZeroString(secret);
    EXPECT_TRUE(secret.empty());
}

TEST(InputValidatorTest, ConstantTimeCompare) {
    EXPECT_TRUE(constantTimeCompare("hello", "hello"));
    EXPECT_FALSE(constantTimeCompare("hello", "world"));
    EXPECT_FALSE(constantTimeCompare("hello", "hellooo"));
    EXPECT_FALSE(constantTimeCompare("hello", "HELLO"));

    // Byte-level comparison
    const int a = 42;
    const int b = 42;
    const int c = 43;
    EXPECT_TRUE(constantTimeCompare(&a, &b, sizeof(int)));
    EXPECT_FALSE(constantTimeCompare(&a, &c, sizeof(int)));
}
