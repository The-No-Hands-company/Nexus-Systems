/*
 * smoke_test.c -- Basic correctness checks for the NexusLang runtime library.
 *
 * Compiled and executed by CMake ctest when NLPL_BUILD_TESTS=ON.
 * Exits with 0 on success, 1 on any failure.
 */

#include "nxl_runtime.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int pass_count = 0;
static int fail_count = 0;

#define EXPECT_EQ_INT(label, got, expected)                             \
    do {                                                                 \
        long long _g = (long long)(got);                                 \
        long long _e = (long long)(expected);                            \
        if (_g == _e) {                                                  \
            printf("PASS  %s\n", label);                                 \
            pass_count++;                                                 \
        } else {                                                         \
            printf("FAIL  %s: got %lld, expected %lld\n",               \
                   label, _g, _e);                                       \
            fail_count++;                                                 \
        }                                                                \
    } while (0)

#define EXPECT_EQ_STR(label, got, expected)                             \
    do {                                                                 \
        const char *_g = (got);                                          \
        const char *_e = (expected);                                     \
        if (_g && _e && strcmp(_g, _e) == 0) {                          \
            printf("PASS  %s\n", label);                                 \
            pass_count++;                                                 \
        } else {                                                         \
            printf("FAIL  %s: got \"%s\", expected \"%s\"\n",           \
                   label, _g ? _g : "(null)", _e ? _e : "(null)");      \
            fail_count++;                                                 \
        }                                                                \
    } while (0)

#define EXPECT_NEAR(label, got, expected, tol)                          \
    do {                                                                 \
        double _d = fabs((double)(got) - (double)(expected));           \
        if (_d <= (double)(tol)) {                                       \
            printf("PASS  %s\n", label);                                 \
            pass_count++;                                                 \
        } else {                                                         \
            printf("FAIL  %s: got %g, expected %g (tolerance %g)\n",   \
                   label, (double)(got), (double)(expected), (double)(tol)); \
            fail_count++;                                                 \
        }                                                                \
    } while (0)


/* --- String tests ---- */
static void test_strings(void)
{
    char *s;

    s = nxl_substr("hello", 1, 3);
    EXPECT_EQ_STR("substr(hello,1,3)", s, "ell");
    free(s);

    s = nxl_charat("hello", 4);
    EXPECT_EQ_STR("charat(hello,4)", s, "o");
    free(s);

    EXPECT_EQ_INT("indexof(hello world, world)", nxl_indexof("hello world", "world"), 6);
    EXPECT_EQ_INT("indexof(hello, xyz)",          nxl_indexof("hello", "xyz"), -1);

    s = nxl_str_replace("aabbcc", "bb", "XX");
    EXPECT_EQ_STR("str_replace", s, "aaXXcc");
    free(s);

    s = nxl_str_trim("  hi  ");
    EXPECT_EQ_STR("str_trim", s, "hi");
    free(s);

    s = nxl_str_toupper("hello");
    EXPECT_EQ_STR("str_toupper", s, "HELLO");
    free(s);

    s = nxl_str_tolower("HELLO");
    EXPECT_EQ_STR("str_tolower", s, "hello");
    free(s);

    s = nxl_str_concat("foo", "bar");
    EXPECT_EQ_STR("str_concat", s, "foobar");
    free(s);

    EXPECT_EQ_INT("str_length", nxl_str_length("hello"), 5);

    s = nxl_str_repeat("ab", 3);
    EXPECT_EQ_STR("str_repeat(ab,3)", s, "ababab");
    free(s);

    EXPECT_EQ_INT("str_starts_with", nxl_str_starts_with("hello", "he"), 1);
    EXPECT_EQ_INT("str_ends_with",   nxl_str_ends_with("hello", "lo"),   1);
    EXPECT_EQ_INT("str_ends_with no", nxl_str_ends_with("hello", "la"), 0);
}


/* --- Math tests ---- */
static void test_math(void)
{
    EXPECT_NEAR("sqrt(4)",       nxl_sqrt(4.0),       2.0,   1e-12);
    EXPECT_NEAR("pow(2,10)",     nxl_pow(2.0, 10.0),  1024.0, 1e-9);
    EXPECT_NEAR("sin(0)",        nxl_sin(0.0),        0.0,   1e-12);
    EXPECT_NEAR("cos(0)",        nxl_cos(0.0),        1.0,   1e-12);
    EXPECT_NEAR("floor(3.7)",    nxl_floor(3.7),      3.0,   1e-12);
    EXPECT_NEAR("ceil(3.2)",     nxl_ceil(3.2),       4.0,   1e-12);
    EXPECT_NEAR("abs_float(-5)", nxl_abs_float(-5.0), 5.0,   1e-12);

    EXPECT_EQ_INT("abs_int(-7)",    nxl_abs_int(-7),      7);
    EXPECT_EQ_INT("min_int(3,5)",   nxl_min_int(3, 5),    3);
    EXPECT_EQ_INT("max_int(3,5)",   nxl_max_int(3, 5),    5);
    EXPECT_EQ_INT("pow_int(2,8)",   nxl_pow_int(2, 8),    256);
    EXPECT_EQ_INT("clamp(10,0,5)",  nxl_clamp_int(10, 0, 5), 5);
    EXPECT_EQ_INT("clamp(-1,0,5)",  nxl_clamp_int(-1, 0, 5), 0);

    EXPECT_EQ_INT("is_nan(nan)", nxl_is_nan(0.0/0.0), 1);
    EXPECT_EQ_INT("is_nan(1.0)", nxl_is_nan(1.0),    0);
}


/* --- Collection tests ---- */
static void test_collections(void)
{
    NLPLArray *arr = nxl_array_new();
    EXPECT_EQ_INT("new array length", nxl_array_length(arr), 0);

    nxl_array_push(arr, 10);
    nxl_array_push(arr, 20);
    nxl_array_push(arr, 30);
    EXPECT_EQ_INT("push->length=3",  nxl_array_length(arr), 3);
    EXPECT_EQ_INT("get(arr,0)==10",  nxl_array_get(arr, 0), 10);
    EXPECT_EQ_INT("get(arr,2)==30",  nxl_array_get(arr, 2), 30);

    int64_t popped = nxl_array_pop(arr);
    EXPECT_EQ_INT("popped==30",      popped, 30);
    EXPECT_EQ_INT("after pop len=2", nxl_array_length(arr), 2);

    NLPLArray *slice = nxl_array_slice(arr, 0, 1);
    EXPECT_EQ_INT("slice len=1",     nxl_array_length(slice), 1);
    EXPECT_EQ_INT("slice[0]==10",    nxl_array_get(slice, 0), 10);
    nxl_array_free(slice);

    nxl_array_push(arr, 5);
    nxl_array_sort(arr);
    EXPECT_EQ_INT("sorted[0]==5",    nxl_array_get(arr, 0), 5);
    EXPECT_EQ_INT("sorted[1]==10",   nxl_array_get(arr, 1), 10);
    EXPECT_EQ_INT("sorted[2]==20",   nxl_array_get(arr, 2), 20);

    EXPECT_EQ_INT("find(10)==1",     nxl_array_find(arr, 10), 1);
    EXPECT_EQ_INT("find(99)==-1",    nxl_array_find(arr, 99), -1);

    nxl_array_free(arr);

    /* Low-level flat helpers */
    int64_t *flat = NULL;
    flat = arrpush(flat, 0, 100);
    flat = arrpush(flat, 1, 200);
    flat = arrpush(flat, 2, 300);
    EXPECT_EQ_INT("flat[0]==100",  flat[0], 100);
    EXPECT_EQ_INT("flat[2]==300",  flat[2], 300);

    int64_t *sliced = arrslice(flat, 1, 3);
    EXPECT_EQ_INT("sliced[0]==200", sliced[0], 200);
    EXPECT_EQ_INT("sliced[1]==300", sliced[1], 300);
    free(sliced);

    int64_t *popped_flat = arrpop(flat, 3);
    EXPECT_EQ_INT("flat popped[0]==100", popped_flat[0], 100);
    free(popped_flat);
    free(flat);
}


/* --- Type conversion tests ---- */
static void test_conversion(void)
{
    char *s;

    s = nxl_int_to_string(42);
    EXPECT_EQ_STR("int_to_string(42)", s, "42");
    free(s);

    s = nxl_int_to_string(-999);
    EXPECT_EQ_STR("int_to_string(-999)", s, "-999");
    free(s);

    s = nxl_bool_to_string(1);
    EXPECT_EQ_STR("bool_to_string(true)", s, "true");
    free(s);

    s = nxl_bool_to_string(0);
    EXPECT_EQ_STR("bool_to_string(false)", s, "false");
    free(s);

    EXPECT_EQ_INT("string_to_int(42)", nxl_string_to_int("42"), 42);
    EXPECT_EQ_INT("string_to_int(-7)", nxl_string_to_int("-7"), -7);
}


/* --- Main ---- */
int main(void)
{
    printf("=== NexusLang stdlib_native smoke test ===\n");

    test_strings();
    test_math();
    test_collections();
    test_conversion();

    printf("=== %d passed, %d failed ===\n", pass_count, fail_count);
    return fail_count > 0 ? 1 : 0;
}
