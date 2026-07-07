#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include "nexus/utility/security/sql_injection_detector.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size == 0) return 0;

    std::string input(reinterpret_cast<const char*>(data), size);

    nexus::utility::security::SqlInjectionDetector::instance().initialize();
    nexus::utility::security::SqlInjectionDetector::instance().detect(input);
    nexus::utility::security::SqlInjectionDetector::instance().shutdown();

    return 0;
}
