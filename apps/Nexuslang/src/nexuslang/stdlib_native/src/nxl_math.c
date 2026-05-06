/*
 * nxl_math.c -- NexusLang math library wrappers.
 *
 * Thin wrappers over libm functions, providing the "nxl_" naming convention
 * and a consistent int64_t / double ABI for the NexusLang runtime.
 *
 * Link with -lm.
 */

#include "../include/nxl_runtime.h"

#include <math.h>
#include <stdlib.h>


/* ===========================================================================
 * Basic transcendental / exponential functions
 * =========================================================================*/

double nxl_sqrt(double x)  { return sqrt(x); }
double nxl_cbrt(double x)  { return cbrt(x); }
double nxl_pow(double x, double y) { return pow(x, y); }
double nxl_exp(double x)   { return exp(x); }
double nxl_log(double x)   { return log(x); }
double nxl_log2(double x)  { return log2(x); }
double nxl_log10(double x) { return log10(x); }


/* ===========================================================================
 * Trigonometric functions
 * =========================================================================*/

double nxl_sin(double x)   { return sin(x); }
double nxl_cos(double x)   { return cos(x); }
double nxl_tan(double x)   { return tan(x); }
double nxl_asin(double x)  { return asin(x); }
double nxl_acos(double x)  { return acos(x); }
double nxl_atan(double x)  { return atan(x); }
double nxl_atan2(double y, double x) { return atan2(y, x); }
double nxl_sinh(double x)  { return sinh(x); }
double nxl_cosh(double x)  { return cosh(x); }
double nxl_tanh(double x)  { return tanh(x); }


/* ===========================================================================
 * Rounding and truncation
 * =========================================================================*/

double nxl_floor(double x) { return floor(x); }
double nxl_ceil(double x)  { return ceil(x);  }
double nxl_round(double x) { return round(x); }
double nxl_trunc(double x) { return trunc(x); }
double nxl_fmod(double x, double y) { return fmod(x, y); }


/* ===========================================================================
 * Absolute value
 * =========================================================================*/

double nxl_abs_float(double x)
{
    return fabs(x);
}

int64_t nxl_abs_int(int64_t x)
{
    /* Avoid undefined behaviour for INT64_MIN by using unsigned arithmetic. */
    return (x < 0) ? (int64_t)(-(uint64_t)x) : x;
}


/* ===========================================================================
 * Min / max
 * =========================================================================*/

int64_t nxl_min_int(int64_t a, int64_t b) { return (a < b) ? a : b; }
int64_t nxl_max_int(int64_t a, int64_t b) { return (a > b) ? a : b; }
double  nxl_min_float(double a, double b) { return (a < b) ? a : b; }
double  nxl_max_float(double a, double b) { return (a > b) ? a : b; }


/* ===========================================================================
 * Classification
 * =========================================================================*/

int nxl_is_nan(double x) { return isnan(x) ? 1 : 0; }
int nxl_is_inf(double x) { return isinf(x) ? 1 : 0; }


/* ===========================================================================
 * Integer exponentiation
 * =========================================================================*/

int64_t nxl_pow_int(int64_t base, int64_t exp)
{
    if (exp < 0) {
        nxl_panic("nxl_pow_int: negative exponent");
    }
    int64_t result = 1;
    while (exp > 0) {
        if (exp & 1) {
            result *= base;
        }
        base *= base;
        exp >>= 1;
    }
    return result;
}


/* ===========================================================================
 * Clamp
 * =========================================================================*/

int64_t nxl_clamp_int(int64_t val, int64_t lo, int64_t hi)
{
    if (val < lo) return lo;
    if (val > hi) return hi;
    return val;
}

double nxl_clamp_float(double val, double lo, double hi)
{
    if (val < lo) return lo;
    if (val > hi) return hi;
    return val;
}
