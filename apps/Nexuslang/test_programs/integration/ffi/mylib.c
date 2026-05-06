#include <stdio.h>

int add_numbers(int a, int b) {
    printf("C Library: Adding %d + %d\n", a, b);
    return a + b;
}

void print_message(const char* msg) {
    if (msg) {
        printf("C Library Message: %s\n", msg);
    } else {
        printf("C Library Message: (null)\n");
    }
}
