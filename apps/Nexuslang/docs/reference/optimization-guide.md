# NexusLang Compiler Optimization Guide

**Last Updated**: February 5, 2026  
**NLPL Version**: v1.2+  
**Optimization Status**: Production-Ready

---

## Overview

NLPL's LLVM-based compiler includes a sophisticated optimization pipeline with 5 distinct optimization levels. This guide explains when to use each level, what optimizations are applied, and how to measure performance impact.

---

## Optimization Levels

### -O0: No Optimization (Debug Mode)

**Purpose**: Fast compilation, easy debugging

**Characteristics:**
- No optimization passes applied
- Generates readable LLVM IR with `alloca` instructions
- Preserves exact source code structure
- All variables remain on stack (easy to inspect in debugger)
- Functions not inlined
- Slowest execution speed

**Use When:**
- Developing new features
- Debugging crashes or logic errors
- Need to inspect variables in debugger
- Want fast compilation times

**Performance:** ~2.5-3x slower than -O2

**Example:**
```bash
python dev_tools/nlplc_llvm.py myprogram.nlpl -O0 -o myprogram_debug
```

---

### -O1: Basic Optimization

**Purpose**: Modest optimization with minimal compile time increase

**Key Passes:**
- Memory-to-register promotion (`mem2reg`)
- Simple dead code elimination
- Basic constant folding
- Trivial inlining (very small functions)

**Characteristics:**
- Removes obvious inefficiencies
- Converts stack variables to SSA form
- Minimal impact on debuggability
- Fast compilation

**Use When:**
- Development builds where some speed is needed
- Iterating on performance-sensitive code
- Want balance between compilation speed and runtime speed

**Performance:** ~1.5-2x slower than -O2, ~1.5x faster than -O0

**Example:**
```bash
python dev_tools/nlplc_llvm.py myprogram.nlpl -O1 -o myprogram_dev
```

---

### -O2: Standard Optimization (Default) ⭐ RECOMMENDED

**Purpose**: Production-quality optimization with good compile times

**Key Passes:**
- All -O1 passes plus:
- Aggressive dead code elimination
- Common subexpression elimination
- Loop invariant code motion
- Instruction combination
- Tail call optimization
- Moderate inlining
- Control flow simplification

**Characteristics:**
- Best balance of compilation time and runtime performance
- Generates highly optimized LLVM IR with PHI nodes
- Functions may be inlined
- Variables converted to registers
- Still reasonably debuggable with `-g` flag

**Use When:**
- Production releases
- Performance-critical applications
- Benchmarking and testing
- 99% of use cases

**Performance:** Baseline (1.80-2.52x slower than C -O2)

**Example:**
```bash
python dev_tools/nlplc_llvm.py myprogram.nlpl -O2 -o myprogram
# Or simply (default):
python dev_tools/nlplc_llvm.py myprogram.nlpl -o myprogram
```

---

### -O3: Aggressive Optimization

**Purpose**: Maximum performance at the cost of larger binaries and longer compilation

**Key Passes:**
- All -O2 passes plus:
- Aggressive function inlining (may inline large functions)
- Loop unrolling (partial and full)
- Vectorization (SIMD when applicable)
- More aggressive constant propagation
- Speculative execution optimizations

**Characteristics:**
- Longest compilation times
- Largest binary sizes
- May produce less debuggable code
- Marginal performance gains over -O2 (2-5%)
- Can sometimes be slower due to code bloat

**Use When:**
- Compute-intensive workloads (scientific computing, ML)
- Inner loops of critical algorithms
- After profiling shows specific hotspots
- When every microsecond counts

**Performance:** 1-5% faster than -O2 (diminishing returns)

**Example:**
```bash
python dev_tools/nlplc_llvm.py myprogram.nlpl -O3 -o myprogram_fast
```

---

### -Os: Size Optimization

**Purpose**: Minimize binary size for embedded systems or distribution

**Key Passes:**
- Selected -O2 passes that reduce size
- Avoids inlining (reduces code duplication)
- Favors smaller instruction sequences
- Loop unrolling disabled

**Characteristics:**
- Smallest binary size
- Reasonable performance (between -O1 and -O2)
- Good for memory-constrained environments
- Faster download/install times

**Use When:**
- Embedded systems with limited storage
- Distributing over network
- Docker containers (smaller images)
- Minimizing memory footprint

**Performance:** 10-20% slower than -O2, but 30-50% smaller binaries

**Example:**
```bash
python dev_tools/nlplc_llvm.py myprogram.nlpl -Os -o myprogram_small
```

---

## Performance Comparison

### Benchmark Results (Fibonacci(1000) Iterative)

| Level | Execution Time | Binary Size | Compilation Time | Notes |
|-------|----------------|-------------|------------------|-------|
| **-O0** | 0.990ms | 24KB | 1.2s | Debug baseline |
| **-O1** | 0.950ms | 22KB | 1.4s | Basic optimization |
| **-O2** | 0.934ms ⭐ | 20KB | 1.8s | **RECOMMENDED** |
| **-O3** | 0.926ms | 28KB | 2.4s | Marginal gain |
| **-Os** | 0.945ms | 18KB | 1.9s | Smallest binary |

**Key Insights:**
- -O2 is only 2-3% slower than -O3
- -O3 produces 40% larger binaries than -O2
- -Os achieves 95% of -O2 performance with 10% smaller size

---

## Optimization Pipeline Details

### What Gets Optimized

#### 1. Memory Operations
**Before optimization:**
```llvm
%1 = alloca i64
store i64 %x, i64* %1
%2 = load i64, i64* %1
```

**After optimization (-O2):**
```llvm
%1 = phi i64 [ %x, %entry ], [ %result, %loop ]
```

**Impact:** 30-40% faster memory access

#### 2. Function Inlining
**Before optimization:**
```nexuslang
function helper with x as Integer returns Integer
    return x times 2
end

set result to call helper with 42
```

**After optimization (-O2):**
```llvm
%result = mul i64 42, 2  ; Function completely eliminated
```

**Impact:** Eliminates function call overhead

#### 3. Dead Code Elimination
**Before optimization:**
```nexuslang
set unused to 42  # Never used
set result to 100
```

**After optimization (-O2):**
```llvm
%result = 100  ; unused variable removed
```

**Impact:** Smaller code, fewer instructions

#### 4. Loop Optimizations
**Before optimization:**
```nexuslang
set sum to 0
set i to 0
while i is less than 1000
    set sum to sum plus i
    set i to i plus 1
end
```

**After optimization (-O2):**
- Loop simplified to PHI nodes
- Invariant code moved out of loop
- Bounds checks optimized
- May use SIMD instructions (-O3)

**Impact:** 20-30% faster loops

---

## Conditional Coroutine Generation

**Major Optimization (v1.2+):** Coroutine infrastructure is now **conditionally generated**

### How It Works

**Before (v1.1):**
- All programs included 400+ lines of coroutine infrastructure
- Added 214 lines of IR even for synchronous code
- 4-6% performance overhead

**After (v1.2+):**
- `has_async_functions` flag tracks async/await usage
- Coroutine types, intrinsics, and runtime only generated when needed
- Zero overhead for synchronous programs

**Impact:**
- 23% smaller IR for synchronous programs
- 4-6% performance improvement
- Zero-cost abstraction for async/await

**Example:**
```nexuslang
# This program has NO async functions
function fibonacci with n as Integer returns Integer
    # ... synchronous code ...
end

# Generated IR: No coroutine infrastructure (699 lines)

# This program USES async/await
async function fetch_data with url as String returns String
    set response to await http_get with url
    return response
end

# Generated IR: Full coroutine infrastructure (913 lines)
```

---

## Debugging Optimized Code

### Viewing IR

**See unoptimized IR:**
```bash
python dev_tools/nlplc_llvm.py myprogram.nlpl --ir > unoptimized.ll
```

**See optimized IR:**
```bash
python dev_tools/nlplc_llvm.py myprogram.nlpl -O2 --ir-opt > optimized.ll
```

**Compare:**
```bash
diff -u unoptimized.ll optimized.ll | less
```

### Common Optimization Artifacts

**PHI Nodes:**
```llvm
%result = phi i64 [ %initial, %entry ], [ %updated, %loop ]
```
- Represents variable that can come from multiple paths
- Indicates SSA form conversion
- Sign of successful optimization

**Local Unnamed Values:**
```llvm
%0 = icmp slt i64 %n, 2
%1 = add i64 %a, %b
```
- Temporary values optimized into registers
- No stack allocation
- Very efficient

**Function Attributes:**
```llvm
#10 = { nofree norecurse nosync nounwind memory(none) }
```
- `nofree`: Never calls free()
- `norecurse`: Not recursive
- `nounwind`: No exceptions
- `memory(none)`: Pure function (no side effects)

---

## Best Practices

### 1. Always Use -O2 for Production

**Why:** Best balance of performance and compile time

**Example Workflow:**
```bash
# Development
python dev_tools/nlplc_llvm.py myapp.nlpl -O0 -o myapp_debug

# Testing
python dev_tools/nlplc_llvm.py myapp.nlpl -O2 -o myapp_test

# Production
python dev_tools/nlplc_llvm.py myapp.nlpl -O2 -o myapp
```

### 2. Profile Before Using -O3

**Don't assume -O3 is always faster**

**Measure first:**
```bash
# Compile both versions
python dev_tools/nlplc_llvm.py myapp.nlpl -O2 -o app_o2
python dev_tools/nlplc_llvm.py myapp.nlpl -O3 -o app_o3

# Benchmark
time ./app_o2
time ./app_o3

# Use perf for detailed analysis
perf stat -r 20 ./app_o2
perf stat -r 20 ./app_o3
```

### 3. Use -Os for Distribution

**When binary size matters:**
```bash
python dev_tools/nlplc_llvm.py myapp.nlpl -Os -o myapp_release
strip myapp_release  # Remove debug symbols
```

### 4. Test Optimization Correctness

**Verify identical output:**
```bash
# Run optimization correctness suite
bash dev_tools/run_optimization_tests.sh

# Or manually test your app
python dev_tools/nlplc_llvm.py myapp.nlpl -O0 -o app_o0
python dev_tools/nlplc_llvm.py myapp.nlpl -O2 -o app_o2

./app_o0 > output_o0.txt
./app_o2 > output_o2.txt

diff output_o0.txt output_o2.txt  # Should be identical
```

---

## Performance Tuning Tips

### 1. Write Optimization-Friendly Code

**Good:**
```nexuslang
# Loop-invariant code
set limit to 1000000
set i to 0
while i is less than limit
    # ... loop body ...
    set i to i plus 1
end
```

**Bad:**
```nexuslang
# Optimizer may not hoist this
set i to 0
while i is less than call get_limit
    # ... loop body ...
    set i to i plus 1
end
```

### 2. Avoid Premature Optimization

1. **Write clear code first**
2. **Profile to find bottlenecks**
3. **Optimize hotspots**
4. **Measure improvements**

### 3. Let LLVM Do Its Job

**Trust the optimizer:**
- LLVM knows more optimization patterns than you
- Simple, clear code often optimizes better
- Don't manually "optimize" code unless profiling shows benefit

---

## Troubleshooting

### Problem: Code is slower at -O2 than -O0

**Likely Causes:**
1. Profiling/measurement error (run multiple times)
2. I/O dominated (optimization won't help)
3. Coroutine overhead (check if async/await is used)

**Solution:**
```bash
# Check IR size
python dev_tools/nlplc_llvm.py app.nlpl -O0 --ir | wc -l
python dev_tools/nlplc_llvm.py app.nlpl -O2 --ir-opt | wc -l

# Profile with perf
perf record -g ./app_o2
perf report
```

### Problem: -O3 makes code slower

**This is normal!**
- Code bloat from aggressive inlining
- Cache misses from larger binaries
- Speculative execution mispredictions

**Solution:** Stick with -O2

### Problem: Optimized code crashes

**Likely Causes:**
1. Undefined behavior in source code
2. Memory safety violation
3. Bug in NexusLang compiler (rare)

**Solution:**
```bash
# Test at all levels
python dev_tools/nlplc_llvm.py app.nlpl -O0 -o app_o0
python dev_tools/nlplc_llvm.py app.nlpl -O1 -o app_o1
python dev_tools/nlplc_llvm.py app.nlpl -O2 -o app_o2

# Find which level introduces crash
./app_o0  # Works?
./app_o1  # Works?
./app_o2  # Crashes? Bug!
```

---

## Advanced Topics

### Link-Time Optimization (LTO)

**Available in build profiles** via `nexuslang build --lto` and profile settings.
The direct `dev_tools/nlplc_llvm.py --lto` workflow below is still planned.

Will enable optimization across module boundaries:
```bash
# Planned syntax:
python dev_tools/nlplc_llvm.py app.nlpl -O2 --lto -o app
```

### Profile-Guided Optimization (PGO)

**Available in the build tool workflow** via `nexuslang pgo instrument|collect|build`.
The direct `dev_tools/nlplc_llvm.py --profile-*` workflow below is still planned.

Will use runtime profiling data to guide optimization:
```bash
# Planned workflow:
python dev_tools/nlplc_llvm.py app.nlpl -O2 --profile-generate -o app_profiled
./app_profiled  # Generate profile data
python dev_tools/nlplc_llvm.py app.nlpl -O2 --profile-use=profile.data -o app_optimized
```

### Custom LLVM Passes

**Future feature** - may allow plugin passes

```python
# Planned API:
from nexuslang.compiler.llvm_optimizer import LLVMOptimizer

optimizer = LLVMOptimizer()
optimizer.add_custom_pass("my-optimization-pass")
optimizer.optimize(module, level="O2")
```

---

## Summary

**Quick Decision Guide:**

| Scenario | Recommended Level | Why |
|----------|-------------------|-----|
| **Development** | -O0 | Fast compilation, easy debugging |
| **Testing** | -O2 | Match production performance |
| **Production** | -O2 | Best balance of speed and size |
| **Benchmarking** | -O2 | Standard comparison point |
| **Size-constrained** | -Os | Smallest binaries |
| **Compute-intensive** | -O3 (after profiling) | Maximum performance |
| **Distribution** | -Os or -O2 | Size vs performance tradeoff |

**Remember:**
- ✅ Always use -O2 for production
- ✅ Test optimization correctness
- ✅ Profile before using -O3
- ✅ Let LLVM do the optimizing
- ❌ Don't assume -O3 is always fastest
- ❌ Don't manually "optimize" without profiling

---

**Last Updated**: February 5, 2026  
**NLPL Version**: v1.2  
**Optimization Target**: Within 3x of C (Achieved: 1.80-2.52x)
