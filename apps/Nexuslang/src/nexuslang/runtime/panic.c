#include <stdio.h>
#include <stdlib.h>

void nxl_panic(char *message) {
    fprintf(stderr, "Panic: %s\n", message);
    exit(1);
}
