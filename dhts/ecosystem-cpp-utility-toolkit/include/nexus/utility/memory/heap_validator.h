#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace nexus::utility::memory {

/**
 * @brief Heap consistency and corruption detection.
 */
class HeapValidator {
public:
    static constexpr uint32_t HEAP_MAGIC = 0xDEADC0DE;
    static constexpr uint8_t FILL_PATTERN_ALLOC = 0xCD;  // Clean memory
    static constexpr uint8_t FILL_PATTERN_FREE = 0xDD;   // Dead memory
    
    /**
     * @brief Validates heap block integrity.
     */
    static bool validateBlock(void* ptr, size_t size) {
        if (!ptr || size == 0) return false;
        
        // Check for common corruption patterns
        uint8_t* bytes = static_cast<uint8_t*>(ptr);
        
        // Check if block looks like freed memory (all 0xDD)
        bool all_freed = true;
        for (size_t i = 0; i < size && i < 64; ++i) {
            if (bytes[i] != FILL_PATTERN_FREE) {
                all_freed = false;
                break;
            }
        }
        
        if (all_freed) {
            return false; // Likely use-after-free
        }
        
        return true;
    }

    /**
     * @brief Fills memory with pattern on allocation.
     */
    static void fillOnAlloc(void* ptr, size_t size) {
        if (ptr && size > 0) {
            std::memset(ptr, FILL_PATTERN_ALLOC, size);
        }
    }

    /**
     * @brief Fills memory with pattern on free.
     */
    static void fillOnFree(void* ptr, size_t size) {
        if (ptr && size > 0) {
            std::memset(ptr, FILL_PATTERN_FREE, size);
        }
    }

    /**
     * @brief Checks for memory corruption patterns.
     */
    static bool detectCorruption(const void* ptr, size_t size) {
        if (!ptr || size == 0) return false;
        
        const uint8_t* bytes = static_cast<const uint8_t*>(ptr);
        
        // Look for suspicious patterns
        std::vector<uint8_t> suspicious = {
            0x00, 0x00, 0x00, 0x00,  // NULL pattern
            0xFF, 0xFF, 0xFF, 0xFF,  // Uninitialized pattern
            FILL_PATTERN_FREE, FILL_PATTERN_FREE, FILL_PATTERN_FREE, FILL_PATTERN_FREE
        };
        
        // Check for long runs of suspicious bytes
        size_t run_length = 0;
        uint8_t last_byte = bytes[0];
        
        for (size_t i = 0; i < size; ++i) {
            if (bytes[i] == last_byte) {
                run_length++;
                if (run_length > 16) {
                    // Long run of same byte might indicate corruption
                    for (uint8_t pattern : suspicious) {
                        if (last_byte == pattern) {
                            return true;
                        }
                    }
                }
            } else {
                run_length = 1;
                last_byte = bytes[i];
            }
        }
        
        return false;
    }

    /**
     * @brief Validates pointer alignment.
     */
    static bool validateAlignment(const void* ptr, size_t alignment) {
        return (reinterpret_cast<uintptr_t>(ptr) % alignment) == 0;
    }

    /**
     * @brief Checks if pointer is in valid address range.
     */
    static bool isValidPointer(const void* ptr) {
        uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
        
        // Check for NULL and very low addresses (likely invalid)
        if (addr < 0x1000) return false;
        
        // Check for very high addresses (likely invalid on most systems)
        if (addr > 0x7FFFFFFFFFFF) return false;
        
        return true;
    }
};

} // namespace nexus::utility::memory
