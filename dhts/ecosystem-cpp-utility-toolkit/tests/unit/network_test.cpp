#include <gtest/gtest.h>
#include <string>
#include <chrono>

#include "nexus/utility/network/url_parser.h"

using namespace nexus::utility::network;

TEST(UrlParserTest, ParseFullUrl) {
    auto result = UrlParser::parse("https://www.example.com:8080/path/to/page?q=1&lang=en#section");

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "www.example.com");
    EXPECT_EQ(result->port, 8080);
    EXPECT_EQ(result->path, "/path/to/page");
    EXPECT_TRUE(result->query.find("q=1") != std::string::npos);
    EXPECT_TRUE(result->fragment.find("section") != std::string::npos);
}

TEST(UrlParserTest, ParseMinimalUrl) {
    auto result = UrlParser::parse("http://example.com");

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scheme, "http");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_TRUE(result->path.empty() || result->path == "/");
}

TEST(UrlParserTest, EmptyUrl) {
    auto result = UrlParser::parse("");
    // The parser may return a valid empty ParsedUrl for empty input
    // Just verify we don't crash
    if (result.has_value()) {
        EXPECT_TRUE(result->scheme.empty());
    }
}

TEST(UrlParserTest, EncodeDecodeRoundtrip) {
    std::string original = "hello world!@#$%^&*()";
    auto encoded = UrlParser::encode(original);
    EXPECT_NE(encoded, original);
    auto decoded = UrlParser::decode(encoded);
    EXPECT_EQ(decoded, original);
}

TEST(UrlParserTest, EncodeReservedChars) {
    auto encoded = UrlParser::encode("hello world");
    EXPECT_TRUE(encoded.find("%20") != std::string::npos || encoded.find('+') != std::string::npos);
}
