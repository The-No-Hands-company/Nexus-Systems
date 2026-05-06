// Iterative Fibonacci benchmark - matches bench_fibonacci.nlpl
#include <stdio.h>
#include <time.h>
#include <stdint.h>

int64_t fibonacci(int n) {
    if (n <= 1) {
        return n;
    }
    
    int64_t a = 0;
    int64_t b = 1;
    
    for (int i = 2; i <= n; i++) {
        int64_t temp = a + b;
        a = b;
        b = temp;
    }
    
    return b;
}

int main() {
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    int64_t result = fibonacci(1000);
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    double elapsed = (end.tv_sec - start.tv_sec) + 
                     (end.tv_nsec - start.tv_nsec) / 1e9;
    
    printf("Fibonacci(1000) = %lld\n", result);
    printf("Time: %.9f seconds\n", elapsed);
    
    return 0;
}
