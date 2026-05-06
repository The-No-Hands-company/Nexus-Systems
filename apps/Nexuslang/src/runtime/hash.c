#include <stdint.h>

/* djb2 hash function */
int64_t hash_string(const char *str) {
    int64_t hash = 5381;
    int c;

    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}

#include <stdlib.h>

/* Wrapper for free to avoid keyword collision in NexusLang */
void c_free(void *ptr) {
    free(ptr);
}
