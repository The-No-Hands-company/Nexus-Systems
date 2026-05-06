// Equivalent C code for Fibonacci benchmark
#include <stdio.h>
#include <time.h>

long long fibonacci(int n) {
    if (n <= 1) {
        return n;
    }
    return fibonacci(n - 1) + fibonacci(n - 2);
}

int main() {
    clock_t start = clock();
    long long result = fibonacci(30);
    clock_t end = clock();
    
    double elapsed = ((double)(end - start)) / CLOCKS_PER_SEC;
    
    printf("Fibonacci(30) = %lld\n", result);
    printf("Time: %.6f seconds\n", elapsed);
    
    return 0;
}
