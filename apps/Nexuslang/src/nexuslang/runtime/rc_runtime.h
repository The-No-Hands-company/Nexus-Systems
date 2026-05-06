/**
 * NexusLang Reference Counted Smart Pointer Runtime Library - Header
 * 
 * Public API for Rc<T>, Weak<T>, and Arc<T> types.
 */

#ifndef NLPL_RC_RUNTIME_H
#define NLPL_RC_RUNTIME_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Rc<T> API - Single-threaded reference counting
 * ============================================================================ */

/**
 * Create a new reference counted pointer.
 * @param size Size of the data to allocate
 * @return Pointer to the data section
 */
void* rc_new(size_t size);

/**
 * Increment the strong reference count (clone).
 * @param data Pointer to the data section
 * @return The same data pointer
 */
void* rc_retain(void* data);

/**
 * Decrement the strong reference count and free if zero.
 * @param data Pointer to the data section
 */
void rc_release(void* data);

/**
 * Get the current strong reference count.
 * @param data Pointer to the data section
 * @return Strong reference count
 */
size_t rc_strong_count(void* data);

/**
 * Get a pointer to the actual data.
 * @param data Pointer to the data section
 * @return The same pointer
 */
void* rc_get_data(void* data);

/* ============================================================================
 * Weak<T> API - Weak references
 * ============================================================================ */

/**
 * Create a weak reference from a strong reference.
 * @param data Pointer to the data section
 * @return Weak reference pointer
 */
void* rc_downgrade(void* data);

/**
 * Try to upgrade a weak reference to a strong reference.
 * @param data Pointer to the data section
 * @return Strong reference if still valid, NULL otherwise
 */
void* rc_upgrade(void* data);

/**
 * Release a weak reference.
 * @param data Pointer to the data section
 */
void weak_release(void* data);

/* ============================================================================
 * Arc<T> API - Atomic reference counting (thread-safe)
 * ============================================================================ */

/**
 * Create a new atomic reference counted pointer.
 * @param size Size of the data to allocate
 * @return Pointer to the data section
 */
void* arc_new(size_t size);

/**
 * Increment the strong reference count atomically.
 * @param data Pointer to the data section
 * @return The same data pointer
 */
void* arc_retain(void* data);

/**
 * Decrement the strong reference count atomically.
 * @param data Pointer to the data section
 */
void arc_release(void* data);

/**
 * Get the current strong reference count atomically.
 * @param data Pointer to the data section
 * @return Strong reference count
 */
size_t arc_strong_count(void* data);

/**
 * Create a weak reference from an Arc.
 * @param data Pointer to the data section
 * @return Weak reference pointer
 */
void* arc_downgrade(void* data);

/**
 * Try to upgrade a weak Arc reference to a strong reference.
 * @param data Pointer to the data section
 * @return Strong reference if still valid, NULL otherwise
 */
void* arc_upgrade(void* data);

/**
 * Release a weak Arc reference.
 * @param data Pointer to the data section
 */
void arc_weak_release(void* data);

/* ============================================================================
 * Debug and Utility Functions
 * ============================================================================ */

/**
 * Print debug information about an Rc.
 * @param data Pointer to the data section
 * @param label Label for the debug output
 */
void rc_debug(void* data, const char* label);

/**
 * Print debug information about an Arc.
 * @param data Pointer to the data section
 * @param label Label for the debug output
 */
void arc_debug(void* data, const char* label);

#ifdef __cplusplus
}
#endif

#endif /* NLPL_RC_RUNTIME_H */
