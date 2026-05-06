#ifndef TEST_EXPORT_H
#define TEST_EXPORT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* Exported Functions */
int64_t calc_add(int64_t x, int64_t y);
int64_t calc_sub(int64_t x, int64_t y);

#ifdef __cplusplus
}
#endif

#endif /* TEST_EXPORT_H */