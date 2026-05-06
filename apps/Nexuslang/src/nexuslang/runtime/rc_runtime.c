/**
 * NexusLang Reference Counted Smart Pointer Runtime Library
 * 
 * Provides reference counting functionality for NexusLang Rc<T>, Weak<T>, and Arc<T> types.
 * 
 * Memory Layout:
 * ┌──────────────┬──────────────┬──────────────┐
 * │ strong_count │ weak_count   │ data         │
 * │ (size_t)     │ (size_t)     │ (T)          │
 * └──────────────┴──────────────┴──────────────┘
 * 
 * The pointer returned to NexusLang code points to the data section.
 * The refcounts are stored at negative offsets from the data pointer.
 */

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdatomic.h>

/* Internal structure for Rc metadata */
typedef struct {
    size_t strong_count;  /* Strong reference count */
    size_t weak_count;    /* Weak reference count */
} RcMetadata;

/* Internal structure for Arc metadata (thread-safe) */
typedef struct {
    _Atomic size_t strong_count;
    _Atomic size_t weak_count;
} ArcMetadata;

/* Get metadata pointer from data pointer */
static inline RcMetadata* rc_get_metadata(void* data) {
    return (RcMetadata*)((char*)data - sizeof(RcMetadata));
}

/* Get Arc metadata pointer from data pointer */
static inline ArcMetadata* arc_get_metadata(void* data) {
    return (ArcMetadata*)((char*)data - sizeof(ArcMetadata));
}

/* ============================================================================
 * Rc<T> Implementation (Single-threaded reference counting)
 * ============================================================================ */

/**
 * Create a new reference counted pointer.
 * 
 * @param size Size of the data to allocate
 * @return Pointer to the data section (not the metadata)
 */
void* rc_new(size_t size) {
    /* Allocate metadata + data */
    size_t total_size = sizeof(RcMetadata) + size;
    RcMetadata* metadata = (RcMetadata*)malloc(total_size);
    
    if (!metadata) {
        fprintf(stderr, "NLPL Runtime Error: Failed to allocate memory for Rc\n");
        exit(1);
    }
    
    /* Initialize metadata */
    metadata->strong_count = 1;
    metadata->weak_count = 0;
    
    /* Zero-initialize data section */
    void* data = (char*)metadata + sizeof(RcMetadata);
    memset(data, 0, size);
    
    return data;
}

/**
 * Increment the strong reference count (clone).
 * 
 * @param data Pointer to the data section
 * @return The same data pointer (for convenience)
 */
void* rc_retain(void* data) {
    if (!data) return NULL;
    
    RcMetadata* metadata = rc_get_metadata(data);
    metadata->strong_count++;
    
    return data;
}

/**
 * Decrement the strong reference count and free if zero.
 * 
 * @param data Pointer to the data section
 */
void rc_release(void* data) {
    if (!data) return;
    
    RcMetadata* metadata = rc_get_metadata(data);
    metadata->strong_count--;
    
    if (metadata->strong_count == 0) {
        /* No more strong references - free data if no weak references */
        if (metadata->weak_count == 0) {
            free(metadata);
        } else {
            /* Keep metadata alive for weak references, but mark data as invalid */
            /* In a full implementation, we'd have a separate allocation for data */
        }
    }
}

/**
 * Get the current strong reference count.
 * 
 * @param data Pointer to the data section
 * @return Strong reference count
 */
size_t rc_strong_count(void* data) {
    if (!data) return 0;
    
    RcMetadata* metadata = rc_get_metadata(data);
    return metadata->strong_count;
}

/**
 * Get a pointer to the actual data (identity function for compatibility).
 * 
 * @param data Pointer to the data section
 * @return The same pointer
 */
void* rc_get_data(void* data) {
    return data;
}

/* ============================================================================
 * Weak<T> Implementation (Weak references)
 * ============================================================================ */

/**
 * Create a weak reference from a strong reference.
 * 
 * @param data Pointer to the data section
 * @return Pointer to the data section (weak reference)
 */
void* rc_downgrade(void* data) {
    if (!data) return NULL;
    
    RcMetadata* metadata = rc_get_metadata(data);
    metadata->weak_count++;
    
    return data;
}

/**
 * Try to upgrade a weak reference to a strong reference.
 * 
 * @param data Pointer to the data section
 * @return Pointer to data if still valid, NULL if data was deallocated
 */
void* rc_upgrade(void* data) {
    if (!data) return NULL;
    
    RcMetadata* metadata = rc_get_metadata(data);
    
    if (metadata->strong_count > 0) {
        /* Data is still alive, create strong reference */
        metadata->strong_count++;
        return data;
    }
    
    /* Data was deallocated */
    return NULL;
}

/**
 * Release a weak reference.
 * 
 * @param data Pointer to the data section
 */
void weak_release(void* data) {
    if (!data) return;
    
    RcMetadata* metadata = rc_get_metadata(data);
    metadata->weak_count--;
    
    if (metadata->strong_count == 0 && metadata->weak_count == 0) {
        /* No references left at all - free everything */
        free(metadata);
    }
}

/* ============================================================================
 * Arc<T> Implementation (Atomic reference counting for thread safety)
 * ============================================================================ */

/**
 * Create a new atomic reference counted pointer.
 * 
 * @param size Size of the data to allocate
 * @return Pointer to the data section
 */
void* arc_new(size_t size) {
    /* Allocate metadata + data */
    size_t total_size = sizeof(ArcMetadata) + size;
    ArcMetadata* metadata = (ArcMetadata*)malloc(total_size);
    
    if (!metadata) {
        fprintf(stderr, "NLPL Runtime Error: Failed to allocate memory for Arc\n");
        exit(1);
    }
    
    /* Initialize atomic metadata */
    atomic_store(&metadata->strong_count, 1);
    atomic_store(&metadata->weak_count, 0);
    
    /* Zero-initialize data section */
    void* data = (char*)metadata + sizeof(ArcMetadata);
    memset(data, 0, size);
    
    return data;
}

/**
 * Increment the strong reference count atomically.
 * 
 * @param data Pointer to the data section
 * @return The same data pointer
 */
void* arc_retain(void* data) {
    if (!data) return NULL;
    
    ArcMetadata* metadata = arc_get_metadata(data);
    atomic_fetch_add(&metadata->strong_count, 1);
    
    return data;
}

/**
 * Decrement the strong reference count atomically and free if zero.
 * 
 * @param data Pointer to the data section
 */
void arc_release(void* data) {
    if (!data) return;
    
    ArcMetadata* metadata = arc_get_metadata(data);
    size_t old_count = atomic_fetch_sub(&metadata->strong_count, 1);
    
    if (old_count == 1) {
        /* This was the last strong reference */
        size_t weak_count = atomic_load(&metadata->weak_count);
        
        if (weak_count == 0) {
            free(metadata);
        }
    }
}

/**
 * Get the current strong reference count atomically.
 * 
 * @param data Pointer to the data section
 * @return Strong reference count
 */
size_t arc_strong_count(void* data) {
    if (!data) return 0;
    
    ArcMetadata* metadata = arc_get_metadata(data);
    return atomic_load(&metadata->strong_count);
}

/**
 * Create a weak reference from an Arc.
 * 
 * @param data Pointer to the data section
 * @return Weak reference pointer
 */
void* arc_downgrade(void* data) {
    if (!data) return NULL;
    
    ArcMetadata* metadata = arc_get_metadata(data);
    atomic_fetch_add(&metadata->weak_count, 1);
    
    return data;
}

/**
 * Try to upgrade a weak Arc reference to a strong reference.
 * 
 * @param data Pointer to the data section
 * @return Strong reference if still valid, NULL otherwise
 */
void* arc_upgrade(void* data) {
    if (!data) return NULL;
    
    ArcMetadata* metadata = arc_get_metadata(data);
    
    /* Use compare-and-swap loop to safely increment strong count */
    size_t old_count = atomic_load(&metadata->strong_count);
    
    while (old_count > 0) {
        if (atomic_compare_exchange_weak(&metadata->strong_count, &old_count, old_count + 1)) {
            /* Successfully upgraded */
            return data;
        }
        /* CAS failed, old_count was updated - try again */
    }
    
    /* Strong count is zero - can't upgrade */
    return NULL;
}

/**
 * Release a weak Arc reference.
 * 
 * @param data Pointer to the data section
 */
void arc_weak_release(void* data) {
    if (!data) return;
    
    ArcMetadata* metadata = arc_get_metadata(data);
    size_t old_weak = atomic_fetch_sub(&metadata->weak_count, 1);
    
    if (old_weak == 1) {
        /* This was the last weak reference */
        size_t strong_count = atomic_load(&metadata->strong_count);
        
        if (strong_count == 0) {
            /* No references left - free everything */
            free(metadata);
        }
    }
}

/* ============================================================================
 * Debug and Utility Functions
 * ============================================================================ */

/**
 * Print debug information about an Rc.
 * 
 * @param data Pointer to the data section
 * @param label Label for the debug output
 */
void rc_debug(void* data, const char* label) {
    if (!data) {
        printf("[Rc Debug] %s: NULL\n", label);
        return;
    }
    
    RcMetadata* metadata = rc_get_metadata(data);
    printf("[Rc Debug] %s: strong=%zu, weak=%zu, data=%p\n",
           label, metadata->strong_count, metadata->weak_count, data);
}

/**
 * Print debug information about an Arc.
 * 
 * @param data Pointer to the data section
 * @param label Label for the debug output
 */
void arc_debug(void* data, const char* label) {
    if (!data) {
        printf("[Arc Debug] %s: NULL\n", label);
        return;
    }
    
    ArcMetadata* metadata = arc_get_metadata(data);
    size_t strong = atomic_load(&metadata->strong_count);
    size_t weak = atomic_load(&metadata->weak_count);
    
    printf("[Arc Debug] %s: strong=%zu, weak=%zu, data=%p\n",
           label, strong, weak, data);
}
