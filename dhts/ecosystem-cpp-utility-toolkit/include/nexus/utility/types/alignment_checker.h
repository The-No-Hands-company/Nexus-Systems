#pragma once

#include <cstdint>
#include <concepts>
#include <source_location>
#include <stdexcept>
#include <string>

namespace nexus::utility::types {

/**
 * @brief Utilities to verify pointer alignment.
 */
class AlignmentChecker {
public:
    /**
     * @brief Checks if a pointer is aligned to specific byte boundary.
     * @param ptr Pointer to check
     * @param alignment Must be a power of 2
     */
    static bool isAligned(const void* ptr, std::size_t alignment) {
        if (alignment == 0) return false;
        return (reinterpret_cast<std::uintptr_t>(ptr) & (alignment - 1)) == 0;
    }

    /**
     * @brief Checks if a pointer is aligned suitably for type T.
     */
    template<typename T>
    static bool isAlignedFor(const void* ptr) {
        return isAligned(ptr, alignof(T));
    }

    /**
     * @brief Asserts that a pointer is aligned, throwing std::runtime_error if not.
     */
    template<typename T>
    static void assertAligned(const void* ptr, std::source_location loc = std::source_location::current()) {
        if (!isAlignedFor<T>(ptr)) {
            // Error formatting simplified
            throw std::runtime_error("Alignment check failed in " + std::string(loc.function_name()));
        }
    }
    
    /**
     * @brief Helper to align a pointer forward to the next multiple of alignment.
     * Dangerous if buffer size not checked, use with caution.
     */
    static void* alignForward(void* ptr, std::size_t alignment) {
        std::uintptr_t addr = reinterpret_cast<std::uintptr_t>(ptr);
        std::uintptr_t aligned_addr = (addr + (alignment - 1)) & ~(alignment - 1);
        return reinterpret_cast<void*>(aligned_addr);
    }
};

} // namespace nexus::utility::types
