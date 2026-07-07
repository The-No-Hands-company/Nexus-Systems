#pragma once

#include <cstddef>
#include <cstdint>

// Sanitizer detection and annotations
#if defined(__has_feature)
  #if __has_feature(address_sanitizer)
    #define NEXUS_HAS_ASAN 1
  #endif
  #if __has_feature(thread_sanitizer)
    #define NEXUS_HAS_TSAN 1
  #endif
  #if __has_feature(memory_sanitizer)
    #define NEXUS_HAS_MSAN 1
  #endif
  #if __has_feature(undefined_behavior_sanitizer)
    #define NEXUS_HAS_UBSAN 1
  #endif
#endif

#if defined(__SANITIZE_ADDRESS__)
  #define NEXUS_HAS_ASAN 1
#endif

#if defined(__SANITIZE_THREAD__)
  #define NEXUS_HAS_TSAN 1
#endif

// Include sanitizer interfaces if available
#ifdef NEXUS_HAS_ASAN
  #include <sanitizer/asan_interface.h>
#endif

#ifdef NEXUS_HAS_TSAN
  #include <sanitizer/tsan_interface.h>
#endif

#ifdef NEXUS_HAS_MSAN
  #include <sanitizer/msan_interface.h>
#endif

namespace nexus::utility::memory {

/**
 * @brief Wrapper for memory sanitizer integration.
 */
class MemorySanitizerWrapper {
public:
    /**
     * @brief Checks if running under AddressSanitizer.
     */
    static constexpr bool hasASan() {
#ifdef NEXUS_HAS_ASAN
        return true;
#else
        return false;
#endif
    }

    /**
     * @brief Checks if running under ThreadSanitizer.
     */
    static constexpr bool hasTSan() {
#ifdef NEXUS_HAS_TSAN
        return true;
#else
        return false;
#endif
    }

    /**
     * @brief Checks if running under MemorySanitizer.
     */
    static constexpr bool hasMSan() {
#ifdef NEXUS_HAS_MSAN
        return true;
#else
        return false;
#endif
    }

    /**
     * @brief Checks if running under UndefinedBehaviorSanitizer.
     */
    static constexpr bool hasUBSan() {
#ifdef NEXUS_HAS_UBSAN
        return true;
#else
        return false;
#endif
    }

    /**
     * @brief Poisons a memory region (ASan).
     */
    static void poisonMemory(const void* addr, size_t size) {
#ifdef NEXUS_HAS_ASAN
        __asan_poison_memory_region(addr, size);
#else
        (void)addr; (void)size;
#endif
    }

    /**
     * @brief Unpoisons a memory region (ASan).
     */
    static void unpoisonMemory(const void* addr, size_t size) {
#ifdef NEXUS_HAS_ASAN
        __asan_unpoison_memory_region(addr, size);
#else
        (void)addr; (void)size;
#endif
    }

    /**
     * @brief Marks memory as uninitialized (MSan).
     */
    static void markUninitialized(const void* addr, size_t size) {
#ifdef NEXUS_HAS_MSAN
        __msan_allocated_memory(addr, size);
#else
        (void)addr; (void)size;
#endif
    }

    /**
     * @brief Checks if memory is poisoned (ASan).
     */
    static bool isMemoryPoisoned(const void* addr, size_t size) {
#ifdef NEXUS_HAS_ASAN
        return __asan_region_is_poisoned(const_cast<void*>(addr), size) != nullptr;
#else
        (void)addr; (void)size;
        return false;
#endif
    }

    /**
     * @brief Annotates happens-before relationship (TSan).
     */
    static void annotateHappensBefore(const void* addr) {
#ifdef NEXUS_HAS_TSAN
        __tsan_release(const_cast<void*>(addr));
#else
        (void)addr;
#endif
    }

    /**
     * @brief Annotates happens-after relationship (TSan).
     */
    static void annotateHappensAfter(const void* addr) {
#ifdef NEXUS_HAS_TSAN
        __tsan_acquire(const_cast<void*>(addr));
#else
        (void)addr;
#endif
    }

    /**
     * @brief Gets sanitizer name if running under one.
     */
    static const char* getSanitizerName() {
        if (hasASan()) return "AddressSanitizer";
        if (hasTSan()) return "ThreadSanitizer";
        if (hasMSan()) return "MemorySanitizer";
        if (hasUBSan()) return "UndefinedBehaviorSanitizer";
        return "None";
    }
};

} // namespace nexus::utility::memory
