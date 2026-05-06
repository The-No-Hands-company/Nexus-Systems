"""Sieve of Eratosthenes benchmark - matches bench_sieve.nlpl (primes up to 1000)."""
import time


def sieve_of_eratosthenes(limit: int) -> int:
    primes = [True] * (limit + 1)
    primes[0] = False
    if limit >= 1:
        primes[1] = False
    p = 2
    while p * p <= limit:
        if primes[p]:
            multiple = p * p
            while multiple <= limit:
                primes[multiple] = False
                multiple += p
        p += 1
    return sum(1 for i in range(2, limit + 1) if primes[i])


if __name__ == "__main__":
    start = time.perf_counter()
    result = sieve_of_eratosthenes(1000)
    elapsed = time.perf_counter() - start
    print(f"Found {result} primes up to 1000")
    print(f"Time: {elapsed:.9f} seconds")
