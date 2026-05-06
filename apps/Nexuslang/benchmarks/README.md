# NexusLang Benchmarks

Performance testing and optimization benchmarks for the NexusLang compiler.

## Files

### benchmark_simple.py
Performance comparison script testing different optimization levels (-O0, -O2, -O3).

**Usage**:
```bash
python3 benchmark_simple.py
```

**Output**: Execution time statistics and speedup factors for each optimization level.

### benchmark_fib.nlpl
Compute-intensive Fibonacci benchmark program used for performance testing.

Computes the first 30 Fibonacci numbers iteratively to measure execution performance.

## Running Benchmarks

```bash
# From project root
cd benchmarks

# Run optimization level comparison
python3 benchmark_simple.py

# Compile and run Fibonacci benchmark manually
../nlplc benchmark_fib.nlpl -o fib_test --optimize 3 --run
```

## Benchmark Results (Jan 24, 2026)

**Fibonacci Test (30 iterations, 10 runs)**:
- O0 (baseline): 0.84ms
- O2 (optimized): 1.50ms (optimization overhead for small programs)
- O3 (aggressive): 0.76ms - **1.11x speedup over O0**

**Insight**: O3 provides consistent speedup for compute-intensive code. For very small programs, optimization overhead may dominate.
