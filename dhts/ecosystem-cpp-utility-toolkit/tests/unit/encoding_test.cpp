#include <gtest/gtest.h>
#include <string>

#include "nexus/utility/encoding/base64_encoder.h"
#include "nexus/utility/encoding/uuid_generator.h"

using namespace nexus::utility::encoding;

TEST(Base64EncoderTest, EncodeDecode) {
    std::string original = "Hello, World!";
    auto encoded = Base64Encoder::encode(original);
    auto decoded = Base64Encoder::decode(encoded);

    std::string decoded_str(decoded.begin(), decoded.end());
    EXPECT_EQ(decoded_str, original);
    EXPECT_NE(encoded, original);
}

TEST(Base64EncoderTest, EmptyString) {
    auto encoded = Base64Encoder::encode("");
    EXPECT_TRUE(encoded.empty());
    auto decoded = Base64Encoder::decode("");
    EXPECT_TRUE(decoded.empty());
}

TEST(Base64EncoderTest, BinaryData) {
    std::string binary("\x00\x01\x02\x03\xFF\xFE\xFD", 7);
    auto encoded = Base64Encoder::encode(binary);
    auto decoded = Base64Encoder::decode(encoded);
    EXPECT_EQ(decoded.size(), binary.size());
    EXPECT_EQ(std::string(decoded.begin(), decoded.end()), binary);
}

TEST(Base64EncoderTest, EncodeBytes) {
    std::vector<uint8_t> bytes = {0x48, 0x65, 0x6C, 0x6C, 0x6F};
    auto encoded = Base64Encoder::encode(bytes);
    EXPECT_FALSE(encoded.empty());
    auto decoded = Base64Encoder::decode(encoded);
    std::string decoded_str(decoded.begin(), decoded.end());
    EXPECT_EQ(decoded_str, "Hello");
}

TEST(Base64EncoderTest, UrlSafeEncoding) {
    // Test that encode produces standard base64
    auto encoded = Base64Encoder::encode("test");
    EXPECT_FALSE(encoded.empty());
    // Standard base64 should only contain A-Za-z0-9+/=
    for (char c : encoded) {
        bool valid = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                     (c >= '0' && c <= '9') || c == '+' || c == '/' || c == '=';
        EXPECT_TRUE(valid) << "Invalid base64 char: " << c;
    }
}

TEST(UUIDGeneratorTest, ValidFormat) {
    auto uuid = UUIDGenerator::generate();

    // UUID v4 format: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
    EXPECT_EQ(uuid.size(), 36);
    EXPECT_EQ(uuid[8], '-');
    EXPECT_EQ(uuid[13], '-');
    EXPECT_EQ(uuid[18], '-');
    EXPECT_EQ(uuid[23], '-');

    // Check version nibble is 4
    EXPECT_EQ(uuid[14], '4');

    // Check variant nibble is 8/9/a/b
    char variant = uuid[19];
    EXPECT_TRUE(variant == '8' || variant == '9' || variant == 'a' || variant == 'b' ||
                variant == 'A' || variant == 'B');
}

TEST(UUIDGeneratorTest, Uniqueness) {
    std::set<std::string> uuids;
    for (int i = 0; i < 100; ++i) {
        uuids.insert(UUIDGenerator::generate());
    }
    EXPECT_EQ(uuids.size(), 100);
}
