#ifdef NEXUS_ENABLE_GLOBAL_NEW_DELETE

#include "nexus/utility/memory/memory_tracker.h"
#include <new>
#include <cstdlib>

// Helper to avoid infinite recursion if MemoryTracker itself allocates
// Thread-local flag to prevent recursive tracking
static thread_local bool g_tracking_allocation = false;

void* operator new(std::size_t size) {
    void* ptr = std::malloc(size);
    if (!ptr) {
        throw std::bad_alloc();
    }

    if (!g_tracking_allocation) {
        g_tracking_allocation = true;
        try {
            nexus::utility::memory::MemoryTracker::instance().recordAllocation(ptr, size, "global new");
        } catch (...) {
            // recursion or error during tracking
        }
        g_tracking_allocation = false;
    }

    return ptr;
}

void* operator new[](std::size_t size) {
    void* ptr = std::malloc(size);
    if (!ptr) {
        throw std::bad_alloc();
    }

    if (!g_tracking_allocation) {
        g_tracking_allocation = true;
        try {
            nexus::utility::memory::MemoryTracker::instance().recordAllocation(ptr, size, "global new[]");
        } catch (...) {
        }
        g_tracking_allocation = false;
    }

    return ptr;
}

void operator delete(void* ptr) noexcept {
    if (!ptr) return;

    if (!g_tracking_allocation) {
        g_tracking_allocation = true;
        try {
            nexus::utility::memory::MemoryTracker::instance().recordDeallocation(ptr);
        } catch (...) {
        }
        g_tracking_allocation = false;
    }

    std::free(ptr);
}

void operator delete[](void* ptr) noexcept {
    if (!ptr) return;

    if (!g_tracking_allocation) {
        g_tracking_allocation = true;
        try {
            nexus::utility::memory::MemoryTracker::instance().recordDeallocation(ptr);
        } catch (...) {
        }
        g_tracking_allocation = false;
    }

    std::free(ptr);
}

// Sized delete (C++14)
void operator delete(void* ptr, std::size_t size) noexcept {
    operator delete(ptr);
}

void operator delete[](void* ptr, std::size_t size) noexcept {
    operator delete[](ptr);
}

#endif // NEXUS_ENABLE_GLOBAL_NEW_DELETE
