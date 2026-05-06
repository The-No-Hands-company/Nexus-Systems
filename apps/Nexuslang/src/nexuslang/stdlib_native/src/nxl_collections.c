/*
 * nxl_collections.c -- NexusLang growable array / collection runtime.
 *
 * Two layers are provided:
 *
 *   1.  High-level NLPLArray (heap-allocated struct with length + capacity).
 *       Used by the runtime when it manages the full lifetime of a list.
 *
 *   2.  Low-level flat array helpers (arrpush / arrpop / arrslice) that
 *       match the calling convention of the inline IR helpers already
 *       emitted by llvm_ir_generator.py.  These return newly malloc'd
 *       buffers; the caller owns the returned storage.
 */

#include "../include/nxl_runtime.h"

#include <stdlib.h>
#include <string.h>


/* ===========================================================================
 * Internal helpers
 * =========================================================================*/

#define INITIAL_CAPACITY 8

static void _array_grow(NLPLArray *arr, int64_t min_capacity)
{
    int64_t new_cap = arr->capacity ? arr->capacity * 2 : INITIAL_CAPACITY;
    while (new_cap < min_capacity) {
        new_cap *= 2;
    }
    int64_t *new_data = (int64_t *)realloc(arr->data,
                                            (size_t)new_cap * sizeof(int64_t));
    if (!new_data) {
        nxl_panic("NLPLArray: memory allocation failed during growth");
    }
    arr->data     = new_data;
    arr->capacity = new_cap;
}

/* qsort comparator for int64_t. */
static int _cmp_i64(const void *a, const void *b)
{
    int64_t x = *(const int64_t *)a;
    int64_t y = *(const int64_t *)b;
    if (x < y) return -1;
    if (x > y) return  1;
    return 0;
}


/* ===========================================================================
 * High-level NLPLArray API
 * =========================================================================*/

NLPLArray *nxl_array_new(void)
{
    NLPLArray *arr = (NLPLArray *)malloc(sizeof(NLPLArray));
    if (!arr) {
        nxl_panic("nxl_array_new: memory allocation failed");
    }
    arr->data     = NULL;
    arr->length   = 0;
    arr->capacity = 0;
    return arr;
}

void nxl_array_free(NLPLArray *arr)
{
    if (!arr) return;
    free(arr->data);
    free(arr);
}

void nxl_array_push(NLPLArray *arr, int64_t elem)
{
    if (!arr) {
        nxl_panic("nxl_array_push: null array");
    }
    if (arr->length >= arr->capacity) {
        _array_grow(arr, arr->length + 1);
    }
    arr->data[arr->length++] = elem;
}

int64_t nxl_array_pop(NLPLArray *arr)
{
    if (!arr || arr->length == 0) {
        nxl_panic("pop from empty array");
    }
    return arr->data[--arr->length];
}

int64_t nxl_array_get(const NLPLArray *arr, int64_t idx)
{
    if (!arr) {
        nxl_panic("nxl_array_get: null array");
    }
    if (idx < 0 || idx >= arr->length) {
        nxl_panic("array index out of range");
    }
    return arr->data[idx];
}

void nxl_array_set(NLPLArray *arr, int64_t idx, int64_t val)
{
    if (!arr) {
        nxl_panic("nxl_array_set: null array");
    }
    if (idx < 0 || idx >= arr->length) {
        nxl_panic("array index out of range");
    }
    arr->data[idx] = val;
}

int64_t nxl_array_length(const NLPLArray *arr)
{
    if (!arr) return 0;
    return arr->length;
}

NLPLArray *nxl_array_slice(const NLPLArray *arr, int64_t start, int64_t end)
{
    if (!arr) {
        nxl_panic("nxl_array_slice: null array");
    }
    if (start < 0 || end < start || end > arr->length) {
        nxl_panic("array slice index out of range");
    }
    int64_t new_len = end - start;
    NLPLArray *result = nxl_array_new();
    if (new_len > 0) {
        _array_grow(result, new_len);
        memcpy(result->data, arr->data + start,
               (size_t)new_len * sizeof(int64_t));
        result->length = new_len;
    }
    return result;
}

NLPLArray *nxl_array_copy(const NLPLArray *arr)
{
    if (!arr) {
        nxl_panic("nxl_array_copy: null array");
    }
    NLPLArray *result = nxl_array_new();
    if (arr->length > 0) {
        _array_grow(result, arr->length);
        memcpy(result->data, arr->data,
               (size_t)arr->length * sizeof(int64_t));
        result->length = arr->length;
    }
    return result;
}

void nxl_array_sort(NLPLArray *arr)
{
    if (!arr || arr->length <= 1) return;
    qsort(arr->data, (size_t)arr->length, sizeof(int64_t), _cmp_i64);
}

void nxl_array_reverse(NLPLArray *arr)
{
    if (!arr || arr->length <= 1) return;
    int64_t lo = 0, hi = arr->length - 1;
    while (lo < hi) {
        int64_t tmp    = arr->data[lo];
        arr->data[lo]  = arr->data[hi];
        arr->data[hi]  = tmp;
        lo++;
        hi--;
    }
}

int64_t nxl_array_find(const NLPLArray *arr, int64_t val)
{
    if (!arr) return -1;
    for (int64_t i = 0; i < arr->length; i++) {
        if (arr->data[i] == val) return i;
    }
    return -1;
}


/* ===========================================================================
 * Low-level flat-array helpers  (IR-compatible calling convention)
 *
 * Each function receives the current flat int64_t* buffer and element count.
 * It allocates a NEW buffer for the result and returns it.
 * The caller is responsible for freeing both the old and new buffers
 * at the appropriate time.
 * =========================================================================*/

int64_t *arrpush(int64_t *arr, int64_t count, int64_t elem)
{
    int64_t new_count  = count + 1;
    int64_t *new_arr   = (int64_t *)malloc((size_t)new_count * sizeof(int64_t));
    if (!new_arr) {
        nxl_panic("arrpush: memory allocation failed");
    }
    if (arr && count > 0) {
        memcpy(new_arr, arr, (size_t)count * sizeof(int64_t));
    }
    new_arr[count] = elem;
    return new_arr;
}

int64_t *arrpop(int64_t *arr, int64_t count)
{
    if (!arr || count <= 0) {
        nxl_panic("arrpop: pop from empty array");
    }
    int64_t new_count = count - 1;
    if (new_count == 0) {
        /* Return a minimal allocation to avoid returning NULL. */
        int64_t *new_arr = (int64_t *)malloc(sizeof(int64_t));
        if (!new_arr) {
            nxl_panic("arrpop: memory allocation failed");
        }
        return new_arr;
    }
    int64_t *new_arr = (int64_t *)malloc((size_t)new_count * sizeof(int64_t));
    if (!new_arr) {
        nxl_panic("arrpop: memory allocation failed");
    }
    memcpy(new_arr, arr, (size_t)new_count * sizeof(int64_t));
    return new_arr;
}

int64_t *arrslice(int64_t *arr, int64_t start, int64_t end)
{
    if (!arr) {
        nxl_panic("arrslice: null array");
    }
    if (start < 0 || end < start) {
        nxl_panic("arrslice: invalid range");
    }
    int64_t len = end - start;
    if (len == 0) {
        int64_t *new_arr = (int64_t *)malloc(sizeof(int64_t));
        if (!new_arr) {
            nxl_panic("arrslice: memory allocation failed");
        }
        return new_arr;
    }
    int64_t *new_arr = (int64_t *)malloc((size_t)len * sizeof(int64_t));
    if (!new_arr) {
        nxl_panic("arrslice: memory allocation failed");
    }
    memcpy(new_arr, arr + start, (size_t)len * sizeof(int64_t));
    return new_arr;
}
