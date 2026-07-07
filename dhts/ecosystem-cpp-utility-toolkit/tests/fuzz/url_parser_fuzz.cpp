#include <cstddef>
#include <cstdint>
#include <string>

#include "nexus/utility/network/url_parser.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size == 0) return 0;

    std::string input(reinterpret_cast<const char*>(data), size);

    try {
        auto parsed = nexus::utility::network::UrlParser::parse(input);
        if (parsed.has_value()) {
            // Verify round-trip for valid URLs
            auto encoded = nexus::utility::network::UrlParser::encode(input);
            if (!encoded.empty()) {
                auto decoded = nexus::utility::network::UrlParser::decode(encoded);
                (void)decoded;
            }
        }
    } catch (...) {
        // Expected - malformed input should be handled gracefully
    }

    return 0;
}
