#include <cstddef>
#include <cstdint>
#include <string>
#include <sstream>

#include "nexus/utility/serialization/json_serializer.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size == 0) return 0;

    std::string input(reinterpret_cast<const char*>(data), size);

    try {
        auto parsed = nexus::utility::serialization::JsonSerializer::parse(input);
        // If parsing succeeds, try to serialize it back
        std::ostringstream oss;
        nexus::utility::serialization::JsonSerializer::serialize(parsed, oss);
    } catch (...) {
        // Expected - malformed input should be handled gracefully
    }

    return 0;
}
