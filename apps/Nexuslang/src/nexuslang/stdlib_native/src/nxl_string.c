/*
 * nxl_string.c -- NexusLang string operations.
 *
 * All functions that return char * return heap-allocated NUL-terminated
 * strings.  The caller owns the memory and must free it.
 *
 * Function semantics mirror the inline LLVM IR helpers defined in
 * llvm_ir_generator.py so that the library can serve as a drop-in
 * replacement for the inlined versions.
 */

#include "../include/nxl_runtime.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* ===========================================================================
 * nxl_substr
 * =========================================================================*/

char *nxl_substr(const char *str, int64_t start, int64_t length)
{
    if (!str) {
        nxl_panic("nxl_substr: null string");
    }
    int64_t src_len = (int64_t)strlen(str);
    if (start < 0 || length < 0 || start + length > src_len) {
        nxl_panic("substring index out of range");
    }
    char *result = (char *)malloc((size_t)(length + 1));
    if (!result) {
        nxl_panic("nxl_substr: memory allocation failed");
    }
    memcpy(result, str + start, (size_t)length);
    result[length] = '\0';
    return result;
}


/* ===========================================================================
 * nxl_charat
 * =========================================================================*/

char *nxl_charat(const char *str, int64_t index)
{
    if (!str) {
        nxl_panic("nxl_charat: null string");
    }
    int64_t len = (int64_t)strlen(str);
    if (index < 0 || index >= len) {
        nxl_panic("character index out of range");
    }
    char *result = (char *)malloc(2);
    if (!result) {
        nxl_panic("nxl_charat: memory allocation failed");
    }
    result[0] = str[index];
    result[1] = '\0';
    return result;
}


/* ===========================================================================
 * nxl_indexof
 * =========================================================================*/

int64_t nxl_indexof(const char *haystack, const char *needle)
{
    if (!haystack || !needle) {
        return -1;
    }
    const char *p = strstr(haystack, needle);
    if (!p) {
        return -1;
    }
    return (int64_t)(p - haystack);
}


/* ===========================================================================
 * nxl_str_replace
 * =========================================================================*/

char *nxl_str_replace(const char *str, const char *old_sub, const char *new_sub)
{
    if (!str) {
        nxl_panic("nxl_str_replace: null string");
    }
    if (!old_sub || !new_sub) {
        return strdup(str);
    }

    size_t old_len = strlen(old_sub);
    size_t new_len = strlen(new_sub);
    size_t str_len = strlen(str);

    if (old_len == 0) {
        /* Nothing to replace. */
        char *result = strdup(str);
        if (!result) {
            nxl_panic("nxl_str_replace: memory allocation failed");
        }
        return result;
    }

    /* Count occurrences to pre-calculate required buffer size. */
    size_t count = 0;
    const char *pos = str;
    while ((pos = strstr(pos, old_sub)) != NULL) {
        count++;
        pos += old_len;
    }

    if (count == 0) {
        char *result = strdup(str);
        if (!result) {
            nxl_panic("nxl_str_replace: memory allocation failed");
        }
        return result;
    }

    /* Allocate result buffer. */
    size_t result_len = str_len + count * (new_len - old_len);
    char *result = (char *)malloc(result_len + 1);
    if (!result) {
        nxl_panic("nxl_str_replace: memory allocation failed");
    }

    char *dst = result;
    const char *src = str;
    const char *match;
    while ((match = strstr(src, old_sub)) != NULL) {
        size_t prefix_len = (size_t)(match - src);
        memcpy(dst, src, prefix_len);
        dst += prefix_len;
        memcpy(dst, new_sub, new_len);
        dst += new_len;
        src = match + old_len;
    }
    /* Copy remaining tail. */
    size_t tail_len = strlen(src);
    memcpy(dst, src, tail_len);
    dst[tail_len] = '\0';
    return result;
}


/* ===========================================================================
 * nxl_str_trim
 * =========================================================================*/

char *nxl_str_trim(const char *str)
{
    if (!str) {
        nxl_panic("nxl_str_trim: null string");
    }

    /* Find first non-whitespace. */
    const char *start = str;
    while (*start && isspace((unsigned char)*start)) {
        start++;
    }

    /* Find last non-whitespace. */
    const char *end = str + strlen(str);
    while (end > start && isspace((unsigned char)*(end - 1))) {
        end--;
    }

    size_t len = (size_t)(end - start);
    char *result = (char *)malloc(len + 1);
    if (!result) {
        nxl_panic("nxl_str_trim: memory allocation failed");
    }
    memcpy(result, start, len);
    result[len] = '\0';
    return result;
}


/* ===========================================================================
 * nxl_str_toupper / nxl_str_tolower
 * =========================================================================*/

char *nxl_str_toupper(const char *str)
{
    if (!str) {
        nxl_panic("nxl_str_toupper: null string");
    }
    size_t len = strlen(str);
    char *result = (char *)malloc(len + 1);
    if (!result) {
        nxl_panic("nxl_str_toupper: memory allocation failed");
    }
    for (size_t i = 0; i < len; i++) {
        result[i] = (char)toupper((unsigned char)str[i]);
    }
    result[len] = '\0';
    return result;
}

char *nxl_str_tolower(const char *str)
{
    if (!str) {
        nxl_panic("nxl_str_tolower: null string");
    }
    size_t len = strlen(str);
    char *result = (char *)malloc(len + 1);
    if (!result) {
        nxl_panic("nxl_str_tolower: memory allocation failed");
    }
    for (size_t i = 0; i < len; i++) {
        result[i] = (char)tolower((unsigned char)str[i]);
    }
    result[len] = '\0';
    return result;
}


/* ===========================================================================
 * nxl_str_concat
 * =========================================================================*/

char *nxl_str_concat(const char *a, const char *b)
{
    if (!a) a = "";
    if (!b) b = "";
    size_t la = strlen(a);
    size_t lb = strlen(b);
    char *result = (char *)malloc(la + lb + 1);
    if (!result) {
        nxl_panic("nxl_str_concat: memory allocation failed");
    }
    memcpy(result, a, la);
    memcpy(result + la, b, lb);
    result[la + lb] = '\0';
    return result;
}


/* ===========================================================================
 * nxl_str_length
 * =========================================================================*/

int64_t nxl_str_length(const char *str)
{
    if (!str) {
        return 0;
    }
    return (int64_t)strlen(str);
}


/* ===========================================================================
 * nxl_str_compare
 * =========================================================================*/

int64_t nxl_str_compare(const char *a, const char *b)
{
    if (!a && !b) return 0;
    if (!a) return -1;
    if (!b) return  1;
    return (int64_t)strcmp(a, b);
}


/* ===========================================================================
 * nxl_str_repeat
 * =========================================================================*/

char *nxl_str_repeat(const char *str, int64_t times)
{
    if (!str || times <= 0) {
        char *result = (char *)malloc(1);
        if (!result) {
            nxl_panic("nxl_str_repeat: memory allocation failed");
        }
        result[0] = '\0';
        return result;
    }
    size_t len = strlen(str);
    size_t total = len * (size_t)times;
    char *result = (char *)malloc(total + 1);
    if (!result) {
        nxl_panic("nxl_str_repeat: memory allocation failed");
    }
    for (int64_t i = 0; i < times; i++) {
        memcpy(result + (size_t)i * len, str, len);
    }
    result[total] = '\0';
    return result;
}


/* ===========================================================================
 * nxl_str_starts_with / nxl_str_ends_with
 * =========================================================================*/

int nxl_str_starts_with(const char *str, const char *prefix)
{
    if (!str || !prefix) return 0;
    size_t plen = strlen(prefix);
    if (strlen(str) < plen) return 0;
    return (strncmp(str, prefix, plen) == 0) ? 1 : 0;
}

int nxl_str_ends_with(const char *str, const char *suffix)
{
    if (!str || !suffix) return 0;
    size_t slen = strlen(str);
    size_t sflen = strlen(suffix);
    if (slen < sflen) return 0;
    return (strcmp(str + slen - sflen, suffix) == 0) ? 1 : 0;
}


/* ===========================================================================
 * nxl_str_format_int
 * =========================================================================*/

char *nxl_str_format_int(const char *fmt, int64_t n)
{
    if (!fmt) {
        return nxl_int_to_string(n);
    }
    /* Pre-allocate a generous buffer; format strings for integers are
     * unlikely to produce output longer than 256 bytes. */
    char buf[256];
    snprintf(buf, sizeof(buf), fmt, (long long)n);
    char *result = strdup(buf);
    if (!result) {
        nxl_panic("nxl_str_format_int: memory allocation failed");
    }
    return result;
}
