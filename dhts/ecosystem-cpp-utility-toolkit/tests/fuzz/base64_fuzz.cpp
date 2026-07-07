#include <cstddef>
#include <cstdint>
#include <string>

#include "nexus/utility/encoding/base64_encoder.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size == 0) return 0;

    std::string input(reinterpret_cast<const char*>(data), size);

    try {
        auto encoded = nexus::utility::encoding::Base64Encoder::encode(input);
        auto decoded = nexus::utility::encoding::Base64Encoder::decode(encoded);
        // For valid base64 input, decode should not throw
        (void)decoded;
    } catch (...) {
        // Expected for malformed input
    }

    return 0;
}
