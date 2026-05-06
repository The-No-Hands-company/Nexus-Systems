/*
 * nxl_runtime.h -- Public API for the NexusLang native runtime library (libNLPL).
 *
 * This header is the single include for C code that links against libNLPL.
 * All symbols use the "nxl_" prefix to avoid collision with libc names.
 *
 * Build:  cmake -S src/nlpl/stdlib_native -B build/stdlib_native && cmake --build build/stdlib_native
 *
 * Link:   clang ... -Lbuild/stdlib_native -lNLPL -lm
 */

#ifndef NXL_RUNTIME_H
#define NXL_RUNTIME_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ===========================================================================
 * Error handling / core runtime
 * =========================================================================*/

/* Print "Error: <msg>" to stderr and call exit(1). */
void    nxl_panic(const char *msg);

/* Assert a condition; call nxl_panic with msg if false. */
void    nxl_assert(int condition, const char *msg);

/* Execute body(data[i]) in parallel over i in [0, count), then join workers. */
void    nxl_parallel_for_i64(int64_t *data, int64_t count, void (*body)(int64_t), int64_t workers);

/* ===========================================================================
 * Print helpers
 * =========================================================================*/

/* Print string without trailing newline. */
void    nxl_print(const char *s);

/* Print string with trailing newline. */
void    nxl_println(const char *s);

/* Print integer / float / bool followed by newline. */
void    nxl_print_int(int64_t n);
void    nxl_print_float(double d);
void    nxl_print_bool(int b);

/* ===========================================================================
 * Type conversion  (caller must free the returned string with free())
 * =========================================================================*/

char   *nxl_int_to_string(int64_t n);
char   *nxl_float_to_string(double d);
char   *nxl_bool_to_string(int b);

int64_t nxl_string_to_int(const char *s);
double  nxl_string_to_float(const char *s);

/* ===========================================================================
 * String operations  (returned strings are heap-allocated; caller must free)
 * =========================================================================*/

/* Return [start, start+length) of str. Panics on out-of-bounds. */
char   *nxl_substr(const char *str, int64_t start, int64_t length);

/* Return the single character at index. Panics on out-of-bounds. */
char   *nxl_charat(const char *str, int64_t index);

/* Return byte-offset of first occurrence of needle in haystack, or -1. */
int64_t nxl_indexof(const char *haystack, const char *needle);

/* Replace every occurrence of old_sub with new_sub. */
char   *nxl_str_replace(const char *str, const char *old_sub, const char *new_sub);

/* Remove leading and trailing ASCII whitespace. */
char   *nxl_str_trim(const char *str);

/* Convert every ASCII letter to uppercase / lowercase. */
char   *nxl_str_toupper(const char *str);
char   *nxl_str_tolower(const char *str);

/* Concatenate a and b into a new string. */
char   *nxl_str_concat(const char *a, const char *b);

/* Return strlen(str) as int64_t. */
int64_t nxl_str_length(const char *str);

/* Lexicographic comparison: <0, 0, >0. */
int64_t nxl_str_compare(const char *a, const char *b);

/* Returns a new string with str repeated n times. */
char   *nxl_str_repeat(const char *str, int64_t times);

/* Returns 1 if str begins/ends with the given prefix/suffix, 0 otherwise. */
int     nxl_str_starts_with(const char *str, const char *prefix);
int     nxl_str_ends_with(const char *str, const char *suffix);

/* Format an integer into a new string using a printf-style format.
 * Only %d / %i / %ld / %lld format specifiers are safe to use. */
char   *nxl_str_format_int(const char *fmt, int64_t n);

/* ===========================================================================
 * Math operations  (thin wrappers over libm; avoids direct IR 'declare')
 * =========================================================================*/

double  nxl_sqrt(double x);
double  nxl_cbrt(double x);
double  nxl_pow(double x, double y);
double  nxl_exp(double x);
double  nxl_log(double x);
double  nxl_log2(double x);
double  nxl_log10(double x);

double  nxl_sin(double x);
double  nxl_cos(double x);
double  nxl_tan(double x);
double  nxl_asin(double x);
double  nxl_acos(double x);
double  nxl_atan(double x);
double  nxl_atan2(double y, double x);
double  nxl_sinh(double x);
double  nxl_cosh(double x);
double  nxl_tanh(double x);

double  nxl_floor(double x);
double  nxl_ceil(double x);
double  nxl_round(double x);
double  nxl_trunc(double x);
double  nxl_fmod(double x, double y);

double  nxl_abs_float(double x);
int64_t nxl_abs_int(int64_t x);

int64_t nxl_min_int(int64_t a, int64_t b);
int64_t nxl_max_int(int64_t a, int64_t b);
double  nxl_min_float(double a, double b);
double  nxl_max_float(double a, double b);

/* Returns 1 if x is NaN, 0 otherwise. */
int     nxl_is_nan(double x);
/* Returns 1 if x is +inf or -inf, 0 otherwise. */
int     nxl_is_inf(double x);

/* Integer exponentiation: base^exp (exp >= 0). */
int64_t nxl_pow_int(int64_t base, int64_t exp);

/* Clamp: return val clamped to [lo, hi]. */
int64_t nxl_clamp_int(int64_t val, int64_t lo, int64_t hi);
double  nxl_clamp_float(double val, double lo, double hi);

/* ===========================================================================
 * Array / collection types
 * =========================================================================*/

/*
 * NLPLArray -- a growable array of int64_t elements.
 *
 * This is the high-level NLPL-managed array type used by the runtime.
 * Capacity doubles on each growth.  All indices are 0-based.
 */
typedef struct NLPLArray {
	int64_t *data;
	int64_t  length;
	int64_t  capacity;
} NLPLArray;

/* Allocate a new empty NLPLArray.  Caller must call nxl_array_free. */
NLPLArray *nxl_array_new(void);

/* Free array and its data buffer.  Pass NULL to no-op. */
void       nxl_array_free(NLPLArray *arr);

/* Append elem to the end.  Grows automatically. */
void       nxl_array_push(NLPLArray *arr, int64_t elem);

/* Remove and return the last element.  Panics on empty array. */
int64_t    nxl_array_pop(NLPLArray *arr);

/* Read element at idx.  Panics on out-of-bounds. */
int64_t    nxl_array_get(const NLPLArray *arr, int64_t idx);

/* Write element at idx.  Panics on out-of-bounds. */
void       nxl_array_set(NLPLArray *arr, int64_t idx, int64_t val);

/* Return the current number of elements. */
int64_t    nxl_array_length(const NLPLArray *arr);

/* Return a new array containing elements [start, end).  Panics on bad range. */
NLPLArray *nxl_array_slice(const NLPLArray *arr, int64_t start, int64_t end);

/* Return a deep copy. */
NLPLArray *nxl_array_copy(const NLPLArray *arr);

/* Sort in ascending order in-place. */
void       nxl_array_sort(NLPLArray *arr);

/* Reverse in-place. */
void       nxl_array_reverse(NLPLArray *arr);

/* Return index of first occurrence of val, or -1. */
int64_t    nxl_array_find(const NLPLArray *arr, int64_t val);

/*
 * Low-level array helpers that match the function signatures used by the LLVM
 * IR generator (arrpush/arrpop/arrslice).  These operate on plain int64_t*
 * flat arrays (no header), matching the IR-generated calling convention.
 * Each call allocates a fresh buffer; the caller owns the returned pointer.
 */
int64_t *arrpush(int64_t *arr, int64_t count, int64_t elem);
int64_t *arrpop(int64_t *arr, int64_t count);
int64_t *arrslice(int64_t *arr, int64_t start, int64_t end);

/* ===========================================================================
 * I/O operations
 * =========================================================================*/

/* Read one line from stdin (excluding '\n').
 * Returns a heap-allocated string; caller must free.
 * Returns NULL on EOF or error. */
char   *nxl_read_line(void);

/* Read entire file into a heap-allocated NUL-terminated string.
 * Returns NULL on error (file not found, permissions, etc.). */
char   *nxl_read_file(const char *path);

/* Write content to file (truncate if exists; create if not).
 * Returns 0 on success, -1 on error. */
int     nxl_write_file(const char *path, const char *content);

/* Append content to file (create if not exists).
 * Returns 0 on success, -1 on error. */
int     nxl_append_file(const char *path, const char *content);

/* Returns 1 if file exists and is readable, 0 otherwise. */
int     nxl_file_exists(const char *path);

/* Returns file size in bytes, or -1 on error. */
int64_t nxl_file_size(const char *path);

/* ===========================================================================
 * Version / build info
 * =========================================================================*/

/* Returns a static NUL-terminated version string like "1.0.0". */
const char *nxl_runtime_version(void);

#ifdef __cplusplus
}
#endif

#endif /* NXL_RUNTIME_H */
