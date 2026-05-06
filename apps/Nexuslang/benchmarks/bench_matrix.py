"""Matrix sum benchmark - matches bench_matrix.nlpl (200x200 matrix)."""
import time


def matrix_sum(size: int) -> int:
    total = 0
    for i in range(size):
        for j in range(size):
            total += i * size + j
    return total


if __name__ == "__main__":
    start = time.perf_counter()
    result = matrix_sum(200)
    elapsed = time.perf_counter() - start
    print(f"Matrix sum (200x200): {result}")
    print(f"Time: {elapsed:.9f} seconds")
