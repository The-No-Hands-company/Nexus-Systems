/*
 * nxl_runtime.c -- Core NexusLang runtime: error handling, print helpers,
 *                   and type conversion utilities.
 */

#include "../include/nxl_runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>


typedef struct nxl_parallel_chunk {
    int64_t *data;
    int64_t start;
    int64_t end;
    void (*body)(int64_t);
} nxl_parallel_chunk;

typedef struct nxl_parallel_chunk_ctx {
    int64_t *data;
    int64_t start;
    int64_t end;
    void (*body)(int64_t, void *);
    void *ctx;
} nxl_parallel_chunk_ctx;


static void *nxl_parallel_worker(void *arg)
{
    nxl_parallel_chunk *chunk = (nxl_parallel_chunk *)arg;
    int64_t i;

    for (i = chunk->start; i < chunk->end; i++) {
        chunk->body(chunk->data[i]);
    }

    return NULL;
}

static void *nxl_parallel_worker_ctx(void *arg)
{
    nxl_parallel_chunk_ctx *chunk = (nxl_parallel_chunk_ctx *)arg;
    int64_t i;

    for (i = chunk->start; i < chunk->end; i++) {
        chunk->body(chunk->data[i], chunk->ctx);
    }

    return NULL;
}

static char *nxl_strdup_local(const char *src)
{
    size_t len;
    char *dst;

    if (src == NULL) {
        return NULL;
    }

    len = strlen(src) + 1;
    dst = (char *)malloc(len);
    if (dst == NULL) {
        return NULL;
    }

    memcpy(dst, src, len);
    return dst;
}


/* ===========================================================================
 * Option/Result runtime helper ABI
 * =========================================================================*/

int NLPL_Optional_has_value(void *opt)
{
    NLPLOptionalValue *value = (NLPLOptionalValue *)opt;
    if (value == NULL) {
        return 0;
    }
    return value->has_value ? 1 : 0;
}

int32_t NLPL_Optional_get_value_kind(void *opt)
{
    NLPLOptionalValue *value = (NLPLOptionalValue *)opt;
    if (value == NULL || !value->has_value) {
        return NLPL_VALUE_KIND_NONE;
    }
    return (int32_t)value->value_kind;
}

int64_t NLPL_Optional_get_value_i64(void *opt)
{
    NLPLOptionalValue *value = (NLPLOptionalValue *)opt;
    if (value == NULL || !value->has_value || value->value_kind != NLPL_VALUE_KIND_I64) {
        return 0;
    }
    return value->value.i64;
}

double NLPL_Optional_get_value_f64(void *opt)
{
    NLPLOptionalValue *value = (NLPLOptionalValue *)opt;
    if (value == NULL || !value->has_value || value->value_kind != NLPL_VALUE_KIND_F64) {
        return 0.0;
    }
    return value->value.f64;
}

void *NLPL_Optional_get_value_ptr(void *opt)
{
    NLPLOptionalValue *value = (NLPLOptionalValue *)opt;
    if (value == NULL || !value->has_value) {
        return NULL;
    }

    if (value->value_kind == NLPL_VALUE_KIND_PTR) {
        return value->value.ptr;
    }

    return NULL;
}

int NLPL_Result_is_ok(void *res)
{
    NLPLResultValue *value = (NLPLResultValue *)res;
    if (value == NULL) {
        return 0;
    }
    return value->is_ok ? 1 : 0;
}

int32_t NLPL_Result_get_value_kind(void *res)
{
    NLPLResultValue *value = (NLPLResultValue *)res;
    if (value == NULL || !value->is_ok) {
        return NLPL_VALUE_KIND_NONE;
    }
    return (int32_t)value->value_kind;
}

int64_t NLPL_Result_get_value_i64(void *res)
{
    NLPLResultValue *value = (NLPLResultValue *)res;
    if (value == NULL || !value->is_ok || value->value_kind != NLPL_VALUE_KIND_I64) {
        return 0;
    }
    return value->value.i64;
}

double NLPL_Result_get_value_f64(void *res)
{
    NLPLResultValue *value = (NLPLResultValue *)res;
    if (value == NULL || !value->is_ok || value->value_kind != NLPL_VALUE_KIND_F64) {
        return 0.0;
    }
    return value->value.f64;
}

int32_t NLPL_Result_get_error_kind(void *res)
{
    NLPLResultValue *value = (NLPLResultValue *)res;
    if (value == NULL || value->is_ok) {
        return NLPL_VALUE_KIND_NONE;
    }
    return (int32_t)value->error_kind;
}

int64_t NLPL_Result_get_error_i64(void *res)
{
    NLPLResultValue *value = (NLPLResultValue *)res;
    if (value == NULL || value->is_ok || value->error_kind != NLPL_VALUE_KIND_I64) {
        return 0;
    }
    return value->error.i64;
}

double NLPL_Result_get_error_f64(void *res)
{
    NLPLResultValue *value = (NLPLResultValue *)res;
    if (value == NULL || value->is_ok || value->error_kind != NLPL_VALUE_KIND_F64) {
        return 0.0;
    }
    return value->error.f64;
}

void *NLPL_Result_get_value_ptr(void *res)
{
    NLPLResultValue *value = (NLPLResultValue *)res;
    if (value == NULL || !value->is_ok) {
        return NULL;
    }

    if (value->value_kind == NLPL_VALUE_KIND_PTR) {
        return value->value.ptr;
    }

    return NULL;
}

void *NLPL_Result_get_error_ptr(void *res)
{
    NLPLResultValue *value = (NLPLResultValue *)res;
    if (value == NULL || value->is_ok) {
        return NULL;
    }

    if (value->error_kind == NLPL_VALUE_KIND_PTR) {
        return value->error.ptr;
    }

    return NULL;
}


/* ===========================================================================
 * Error handling
 * =========================================================================*/

void nxl_panic(const char *msg)
{
    fprintf(stderr, "Error: %s\n", msg ? msg : "unknown error");
    exit(1);
}

void nxl_assert(int condition, const char *msg)
{
    if (!condition) {
        nxl_panic(msg ? msg : "assertion failed");
    }
}

void nxl_parallel_for_i64(int64_t *data, int64_t count, void (*body)(int64_t), int64_t workers)
{
    int64_t i;
    int64_t thread_count;
    int64_t base;
    int64_t rem;
    int64_t start;
    int64_t launched;
    long cpu_count;
    pthread_t *threads;
    nxl_parallel_chunk *chunks;

    if (body == NULL || data == NULL || count <= 0) {
        return;
    }

    if (workers > 0) {
        thread_count = workers;
    } else {
        cpu_count = sysconf(_SC_NPROCESSORS_ONLN);
        thread_count = (cpu_count > 0) ? (int64_t)cpu_count : 4;
    }

    if (thread_count > count) {
        thread_count = count;
    }

    if (thread_count <= 1) {
        for (i = 0; i < count; i++) {
            body(data[i]);
        }
        return;
    }

    threads = (pthread_t *)malloc(sizeof(pthread_t) * (size_t)thread_count);
    chunks = (nxl_parallel_chunk *)malloc(sizeof(nxl_parallel_chunk) * (size_t)thread_count);
    if (threads == NULL || chunks == NULL) {
        free(threads);
        free(chunks);
        for (i = 0; i < count; i++) {
            body(data[i]);
        }
        return;
    }

    base = count / thread_count;
    rem = count % thread_count;
    start = 0;
    launched = 0;

    for (i = 0; i < thread_count; i++) {
        int64_t chunk_size = base + ((i < rem) ? 1 : 0);

        chunks[i].data = data;
        chunks[i].start = start;
        chunks[i].end = start + chunk_size;
        chunks[i].body = body;
        start += chunk_size;

        if (chunks[i].start >= chunks[i].end) {
            continue;
        }

        if (pthread_create(&threads[launched], NULL, nxl_parallel_worker, &chunks[i]) != 0) {
            int64_t j;
            int64_t k;

            for (j = launched; j < thread_count; j++) {
                if (chunks[j].start >= chunks[j].end) {
                    continue;
                }
                for (k = chunks[j].start; k < chunks[j].end; k++) {
                    body(data[k]);
                }
            }
            break;
        }

        launched++;
    }

    for (i = 0; i < launched; i++) {
        (void)pthread_join(threads[i], NULL);
    }

    free(chunks);
    free(threads);
}

void nxl_parallel_for_ctx_i64(int64_t *data, int64_t count, void (*body)(int64_t, void *), void *ctx, int64_t workers)
{
    int64_t i;
    int64_t thread_count;
    int64_t base;
    int64_t rem;
    int64_t start;
    int64_t launched;
    long cpu_count;
    pthread_t *threads;
    nxl_parallel_chunk_ctx *chunks;

    if (body == NULL || data == NULL || count <= 0) {
        return;
    }

    if (workers > 0) {
        thread_count = workers;
    } else {
        cpu_count = sysconf(_SC_NPROCESSORS_ONLN);
        thread_count = (cpu_count > 0) ? (int64_t)cpu_count : 4;
    }

    if (thread_count > count) {
        thread_count = count;
    }

    if (thread_count <= 1) {
        for (i = 0; i < count; i++) {
            body(data[i], ctx);
        }
        return;
    }

    threads = (pthread_t *)malloc(sizeof(pthread_t) * (size_t)thread_count);
    chunks = (nxl_parallel_chunk_ctx *)malloc(sizeof(nxl_parallel_chunk_ctx) * (size_t)thread_count);
    if (threads == NULL || chunks == NULL) {
        free(threads);
        free(chunks);
        for (i = 0; i < count; i++) {
            body(data[i], ctx);
        }
        return;
    }

    base = count / thread_count;
    rem = count % thread_count;
    start = 0;
    launched = 0;

    for (i = 0; i < thread_count; i++) {
        int64_t chunk_size = base + ((i < rem) ? 1 : 0);

        chunks[i].data = data;
        chunks[i].start = start;
        chunks[i].end = start + chunk_size;
        chunks[i].body = body;
        chunks[i].ctx = ctx;
        start += chunk_size;

        if (chunks[i].start >= chunks[i].end) {
            continue;
        }

        if (pthread_create(&threads[launched], NULL, nxl_parallel_worker_ctx, &chunks[i]) != 0) {
            int64_t j;
            int64_t k;

            for (j = launched; j < thread_count; j++) {
                if (chunks[j].start >= chunks[j].end) {
                    continue;
                }
                for (k = chunks[j].start; k < chunks[j].end; k++) {
                    body(data[k], ctx);
                }
            }
            break;
        }

        launched++;
    }

    for (i = 0; i < launched; i++) {
        (void)pthread_join(threads[i], NULL);
    }

    free(chunks);
    free(threads);
}


/* ===========================================================================
 * Print helpers
 * =========================================================================*/

void nxl_print(const char *s)
{
    if (s) {
        fputs(s, stdout);
    }
}

void nxl_println(const char *s)
{
    if (s) {
        puts(s);          /* puts appends '\n' */
    } else {
        putchar('\n');
    }
}

void nxl_print_int(int64_t n)
{
    printf("%lld\n", (long long)n);
}

void nxl_print_float(double d)
{
    /* Use %g so that 1.0 prints as "1" and 1.5 prints as "1.5",
     * matching Python's default float-to-string behaviour. */
    printf("%g\n", d);
}

void nxl_print_bool(int b)
{
    printf("%s\n", b ? "true" : "false");
}


/* ===========================================================================
 * Type conversion
 * =========================================================================*/

char *nxl_int_to_string(int64_t n)
{
    /* Maximum length of a 64-bit decimal integer (including sign) is 20 digits. */
    char buf[24];
    snprintf(buf, sizeof(buf), "%lld", (long long)n);
    char *result = nxl_strdup_local(buf);
    if (!result) {
        nxl_panic("nxl_int_to_string: memory allocation failed");
    }
    return result;
}

char *nxl_float_to_string(double d)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "%g", d);
    char *result = nxl_strdup_local(buf);
    if (!result) {
        nxl_panic("nxl_float_to_string: memory allocation failed");
    }
    return result;
}

char *nxl_bool_to_string(int b)
{
    char *result = nxl_strdup_local(b ? "true" : "false");
    if (!result) {
        nxl_panic("nxl_bool_to_string: memory allocation failed");
    }
    return result;
}

int64_t nxl_string_to_int(const char *s)
{
    if (!s || *s == '\0') {
        return 0;
    }
    char *end = NULL;
    errno = 0;
    long long val = strtoll(s, &end, 10);
    if (errno != 0 || end == s) {
        return 0;
    }
    return (int64_t)val;
}

double nxl_string_to_float(const char *s)
{
    if (!s || *s == '\0') {
        return 0.0;
    }
    char *end = NULL;
    errno = 0;
    double val = strtod(s, &end);
    if (errno != 0 || end == s) {
        return 0.0;
    }
    return val;
}


/* ===========================================================================
 * Version
 * =========================================================================*/

const char *nxl_runtime_version(void)
{
    return "1.0.0";
}
