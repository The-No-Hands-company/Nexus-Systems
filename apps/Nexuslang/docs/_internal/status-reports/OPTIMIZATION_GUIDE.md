# NexusLang Compiler Optimization Guide

## Overview

The NexusLang compiler (`nlplc`) supports multiple optimization levels to balance compilation time, binary size, and runtime performance.

## Optimization Levels

### `-O0` (Default - No Optimization)
- **Purpose**: Fast compilation, easy debugging
- **Use case**: Development, debugging
- **Characteristics**:
 - Fastest compilation time
 - Largest binary size
 - Slowest execution
 - Preserves all debug information

### `-O1` (Basic Optimization)
- **Purpose**: Balance between speed and compilation time
- **Use case**: Quick builds with some optimization
- **Passes**: mem2reg, simplifycfg, instcombine
- **Characteristics**:
 - ~26% smaller binaries (19KB 14KB)
 - ~4x faster execution
 - Minimal compilation time increase

### `-O2` (Moderate Optimization) Recommended
- **Purpose**: Production builds with good performance
- **Use case**: Release builds, production deployment
- **Passes**: mem2reg, simplifycfg, instcombine, reassociate, gvn, sccp, deadargelim, inline
- **Characteristics**:
 - ~26% smaller binaries
 - ~4x faster execution
 - Inlining enabled
 - Good balance of size and speed

### `-O3` (Aggressive Optimization)
- **Purpose**: Maximum runtime performance
- **Use case**: Performance-critical applications
- **Passes**: All O2 passes + loop-unroll, loop-vectorize, slp-vectorizer, aggressive-instcombine
- **Characteristics**:
 - ~26% smaller binaries
 - ~4x faster execution
 - Loop optimizations and vectorization
 - Slightly longer compilation time

### `-Os` (Size Optimization)
- **Purpose**: Minimize binary size
- **Use case**: Embedded systems, size-constrained environments
- **Passes**: mem2reg, simplifycfg, instcombine, dce, dse
- **Characteristics**:
 - Smallest possible binaries
 - Prioritizes size over speed
 - Good for limited storage

## Usage Examples

```bash
# Development (no optimization)
./nlplc program.nlpl -O0

# Quick testing with basic optimization
./nlplc program.nlpl -O1

# Production release (recommended)
./nlplc program.nlpl -O2 -o myapp

# Maximum performance
./nlplc program.nlpl -O3 -o myapp

# Minimize size
./nlplc program.nlpl -Os -o myapp

# Verbose output to see optimization passes
./nlplc program.nlpl -O2 -v
```

## Optimization Pipeline

The compiler uses a two-stage optimization approach:

1. **LLVM IR Optimization** (if `opt` tool available):
 - Applies optimization passes to LLVM IR
 - Transforms: mem2reg, simplifycfg, instcombine, inline, etc.
 - Falls back gracefully if `opt` not installed

2. **Backend Optimization** (clang/gcc):
 - Always applied during linking phase
 - Passes `-O0`, `-O1`, `-O2`, `-O3`, or `-Os` to compiler
 - Additional architecture-specific optimizations

## Performance Results

### Benchmark: fibonacci(20) Recursive

| Level | Time | Binary Size | Speedup |
|-------|--------|-------------|---------|
| O0 | 4ms | 19KB | 1x |
| O1 | 1ms | 14KB | 4x |
| O2 | 1ms | 14KB | 4x |
| O3 | 1ms | 14KB | 4x |
| Os | 1ms | 14KB | 4x |

*Note: All optimized levels show similar performance for this benchmark. More complex programs may show greater differences.*

## Optimization Pass Details

### Key Transformations

- **mem2reg**: Promotes memory to registers (SSA form)
- **simplifycfg**: Simplifies control flow graph
- **instcombine**: Combines redundant instructions
- **reassociate**: Reassociates expressions for optimization
- **gvn**: Global value numbering (eliminates redundant loads)
- **sccp**: Sparse conditional constant propagation
- **deadargelim**: Removes dead function arguments
- **inline**: Function inlining
- **loop-unroll**: Unrolls loops
- **loop-vectorize**: Auto-vectorization for SIMD
- **slp-vectorizer**: Superword-level parallelism vectorization
- **dce/dse**: Dead code/store elimination

## Recommendations

1. **Development**: Use `-O0` for fastest iteration
2. **Testing**: Use `-O1` for quick verification
3. **Production**: Use `-O2` for best balance ( recommended)
4. **Performance-critical**: Use `-O3` for CPU-intensive code
5. **Embedded/IoT**: Use `-Os` for constrained devices

## Troubleshooting

### "LLVM opt not available" Warning
- **Cause**: `opt` tool not in PATH
- **Impact**: No IR-level optimization (backend optimization still applied)
- **Solution**: Install LLVM tools or rely on clang optimization
- **Status**: Non-fatal - compilation continues successfully

### No Performance Difference
- **Cause**: Program too simple or already optimal
- **Solution**: Test with more complex code (loops, recursion, arithmetic)

## See Also

- Compiler Architecture: `docs/4_architecture/compiler_architecture.md`
- Backend Strategy: `docs/backend_strategy.md`
- Performance Benchmarking: Run `./nlplc examples/benchmark.nlpl -O2 --run`
