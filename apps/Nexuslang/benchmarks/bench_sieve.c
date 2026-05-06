// Prime Sieve - Sieve of Eratosthenes in C
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>

int sieve_of_eratosthenes(int limit) {
    // Create array of booleans
    bool *primes = (bool *)malloc((limit + 1) * sizeof(bool));
    
    // Initialize all as prime
    primes[0] = false;
    primes[1] = false;
    for (int i = 2; i <= limit; i++) {
        primes[i] = true;
    }
    
    // Sieve algorithm
    for (int p = 2; p * p <= limit; p++) {
        if (primes[p]) {
            // Mark multiples as not prime
            for (int multiple = p * p; multiple <= limit; multiple += p) {
                primes[multiple] = false;
            }
        }
    }
    
    // Count primes
    int count = 0;
    for (int i = 2; i <= limit; i++) {
        if (primes[i]) {
            count++;
        }
    }
    
    free(primes);
    return count;
}

int main() {
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    int result = sieve_of_eratosthenes(1000);
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    double elapsed = (end.tv_sec - start.tv_sec) + 
                     (end.tv_nsec - start.tv_nsec) / 1e9;
    
    printf("Found %d primes up to 1000\n", result);
    printf("Time: %.9f seconds\n", elapsed);
    
    return 0;
}
