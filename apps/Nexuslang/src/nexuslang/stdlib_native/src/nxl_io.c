/*
 * nxl_io.c -- NexusLang I/O runtime helpers.
 *
 * Covers:
 *  - Console I/O (read_line)
 *  - File I/O (read_file, write_file, append_file, file_exists, file_size)
 *
 * All functions that return strings return heap-allocated, NUL-terminated
 * buffers; the caller is responsible for calling free().
 *
 * Error reporting follows standard NexusLang convention: non-fatal issues return
 * NULL / -1; fatal / unrecoverable errors call nxl_panic().
 */

#include "../include/nxl_runtime.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>


/* ===========================================================================
 * nxl_read_line
 * =========================================================================*/

char *nxl_read_line(void)
{
    /* Use a dynamic buffer that grows as needed. */
    size_t  capacity = 128;
    size_t  length   = 0;
    char   *buf      = (char *)malloc(capacity);
    if (!buf) {
        nxl_panic("nxl_read_line: memory allocation failed");
    }

    int c;
    while ((c = fgetc(stdin)) != EOF && c != '\n') {
        if (length + 1 >= capacity) {
            capacity *= 2;
            char *new_buf = (char *)realloc(buf, capacity);
            if (!new_buf) {
                free(buf);
                nxl_panic("nxl_read_line: memory reallocation failed");
            }
            buf = new_buf;
        }
        buf[length++] = (char)c;
    }

    if (c == EOF && length == 0) {
        free(buf);
        return NULL;   /* EOF with no data */
    }

    buf[length] = '\0';
    return buf;
}


/* ===========================================================================
 * nxl_read_file
 * =========================================================================*/

char *nxl_read_file(const char *path)
{
    if (!path) return NULL;

    FILE *fp = fopen(path, "rb");
    if (!fp) {
        return NULL;   /* File not found or no permission -- non-fatal */
    }

    /* Get file size. */
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return NULL;
    }
    long size_long = ftell(fp);
    if (size_long < 0) {
        fclose(fp);
        return NULL;
    }
    rewind(fp);

    size_t size = (size_t)size_long;
    char *buf = (char *)malloc(size + 1);
    if (!buf) {
        fclose(fp);
        nxl_panic("nxl_read_file: memory allocation failed");
    }

    size_t read = fread(buf, 1, size, fp);
    fclose(fp);

    if (read != size) {
        free(buf);
        return NULL;
    }

    buf[size] = '\0';
    return buf;
}


/* ===========================================================================
 * nxl_write_file
 * =========================================================================*/

int nxl_write_file(const char *path, const char *content)
{
    if (!path || !content) return -1;

    FILE *fp = fopen(path, "w");
    if (!fp) return -1;

    size_t len = strlen(content);
    size_t written = fwrite(content, 1, len, fp);
    int ok = (written == len) ? 0 : -1;
    fclose(fp);
    return ok;
}


/* ===========================================================================
 * nxl_append_file
 * =========================================================================*/

int nxl_append_file(const char *path, const char *content)
{
    if (!path || !content) return -1;

    FILE *fp = fopen(path, "a");
    if (!fp) return -1;

    size_t len = strlen(content);
    size_t written = fwrite(content, 1, len, fp);
    int ok = (written == len) ? 0 : -1;
    fclose(fp);
    return ok;
}


/* ===========================================================================
 * nxl_file_exists
 * =========================================================================*/

int nxl_file_exists(const char *path)
{
    if (!path) return 0;
    struct stat st;
    return (stat(path, &st) == 0) ? 1 : 0;
}


/* ===========================================================================
 * nxl_file_size
 * =========================================================================*/

int64_t nxl_file_size(const char *path)
{
    if (!path) return -1;
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    return (int64_t)st.st_size;
}
