"""Iterative Fibonacci benchmark - matches bench_fibonacci.nlpl (Fibonacci(1000))."""
import time


def fibonacci(n: int) -> int:
    if n <= 1:
        return n
    a, b = 0, 1
    for _ in range(2, n + 1):
        a, b = b, a + b
    return b


if __name__ == "__main__":
    start = time.perf_counter()
    result = fibonacci(1000)
    elapsed = time.perf_counter() - start
    print(f"Fibonacci(1000) = {result}")
    print(f"Time: {elapsed:.9f} seconds")
