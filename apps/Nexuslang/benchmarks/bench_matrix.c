// Matrix Sum Benchmark in C
#include <stdio.h>
#include <time.h>

long long matrix_sum(int size) {
    long long sum = 0;
    
    for (int i = 0; i < size; i++) {
        for (int j = 0; j < size; j++) {
            // Simulate matrix operations
            long long value = i * size + j;
            sum += value;
        }
    }
    
    return sum;
}

int main() {
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    long long result = matrix_sum(200);
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    double elapsed = (end.tv_sec - start.tv_sec) + 
                     (end.tv_nsec - start.tv_nsec) / 1e9;
    
    printf("Matrix sum (200x200): %lld\n", result);
    printf("Time: %.9f seconds\n", elapsed);
    
    return 0;
}
