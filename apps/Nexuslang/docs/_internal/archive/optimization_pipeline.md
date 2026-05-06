# NexusLang Compiler Optimization Pipeline

**Status**: Phase 3 Week 1 - Foundation  
**Version**: v1.3-dev  
**Last Updated**: February 4, 2026

---

## Overview

NLPL's compilation pipeline includes both **AST-level** and **LLVM-level** optimizations to achieve near-native performance while maintaining code correctness and debuggability.

### Optimization Philosophy

**Two-Level Approach**:
1. **AST-Level** (High-level): Natural language constructs, semantic optimizations
2. **LLVM-Level** (Low-level): Machine code optimizations, architecture-specific

**Design Goals**:
- Performance within 2-5x of C
- Preserve debugging information at -O0
- Gradual optimization levels (-O0 through -O3)
- Safety: optimizations never change program semantics

---

## Compilation Pipeline

```
NLPL Source (.nxl)
    ↓
┌─────────────────────────┐
│   Lexer (Tokenization)  │
└─────────────────────────┘
    ↓
┌─────────────────────────┐
│   Parser (AST Generation) │
└─────────────────────────┘
    ↓
┌─────────────────────────┐
│  AST-Level Optimization │
│  - Constant Folding     │
│  - Dead Code Elimination│
│  - Bounds Check Removal │
└─────────────────────────┘
    ↓
┌─────────────────────────┐
│  Type Checker (Optional)│
└─────────────────────────┘
    ↓
┌─────────────────────────┐
│  LLVM IR Code Generator │
└─────────────────────────┘
    ↓
┌─────────────────────────┐
│  LLVM-Level Optimization│
│  - PassManager Pipeline │
│  - Inlining, Loop Opts  │
│  - Vectorization (O3)   │
└─────────────────────────┘
    ↓
┌─────────────────────────┐
│  Native Code Generation │
└─────────────────────────┘
    ↓
Executable Binary
```

---

## AST-Level Optimizations

**Location**: `src/nlpl/compiler/optimizer.py`

### 1. Constant Folding (`ConstantFolder`)

**What it does**: Evaluates constant expressions at compile time

**Examples**:
```nlpl
# Before
set result to 2 plus 3 times 4

# After (AST transformed)
set result to 14
```

**Benefits**:
- Eliminates runtime arithmetic
- Reduces instruction count
- Enables further optimizations

**Supported Operations**:
- Arithmetic: `+`, `-`, `*`, `/`, `%`, `**`
- Comparisons: `>`, `<`, `==`, `!=`, `>=`, `<=`
- Logical: `and`, `or`, `not`
- Bitwise: `&`, `|`, `^`, `<<`, `>>`
- String concatenation

### 2. Dead Code Elimination (`DeadCodeEliminator`)

**What it does**: Removes unreachable code

**Examples**:
```nlpl
# Before
if true
    print text "Always executed"
else
    print text "Never executed"  # Dead code
end

return result
print text "Unreachable"  # Dead code
```

**Benefits**:
- Smaller binaries
- Faster execution (fewer instructions)
- Cleaner generated IR

**Removes**:
- Code after `return`, `break`, `continue`
- `if false` blocks
- Unreachable functions

### 3. Bounds Check Optimization (`BoundsCheckOptimizer`)

**What it does**: Eliminates provably safe array bounds checks

**Examples**:
```nlpl
# Before
set array to [1, 2, 3, 4, 5]
set value to array[2]  # Bounds check: 0 <= 2 < 5

# After
set value to array[2]  # No runtime check (proven safe)
```

**Strategies**:
- Constant indices with known array size
- Loop induction variables with proven bounds
- Post-guard elimination (after explicit checks)

---

## LLVM-Level Optimizations

**Location**: `src/nlpl/compiler/llvm_optimizer.py`

Uses `llvmlite` binding to LLVM's `PassManager` for production-grade optimization.

### Optimization Levels

| Level | Name | Purpose | Speed | Size | Compile Time |
|-------|------|---------|-------|------|--------------|
| `-O0` | None | Debug-friendly | 1x | Largest | Fastest |
| `-O1` | Basic | Quick optimization | 1.5x | Large | Fast |
| `-O2` | Standard | Production default | 2-3x | Medium | Medium |
| `-O3` | Aggressive | Maximum performance | 3-4x | Larger | Slow |
| `-Os` | Size | Minimize binary size | 2x | Smallest | Medium |

### -O0: No Optimization

**Passes**: None

**Characteristics**:
- No transformation of generated IR
- All debug information preserved
- Fast compilation
- Easy debugging (1:1 source mapping)

**Use Case**: Development, debugging

### -O1: Basic Optimization

**Passes**:
- `mem2reg` - Memory to register promotion
- `simplifycfg` - Control flow simplification
- `instcombine` - Instruction combining
- `reassociate` - Expression reassociation

**Characteristics**:
- Fast compilation (~2x slower than O0)
- Modest speedup (1.5x)
- Minimal code size increase

**Use Case**: Fast iteration during development

### -O2: Standard Optimization (DEFAULT)

**All -O1 passes plus**:
- `inline` - Function inlining (threshold: 225)
- `gvn` - Global value numbering (CSE)
- `sccp` - Sparse conditional constant propagation
- `dce` - Dead code elimination
- `dse` - Dead store elimination
- `loop-simplify` - Loop canonicalization
- `loop-rotate` - Loop rotation
- `licm` - Loop-invariant code motion
- `indvars` - Induction variable simplification

**Characteristics**:
- Balanced compile time vs performance
- 2-3x speedup over -O0
- Moderate code size increase
- Good for production builds

**Use Case**: Production releases, benchmarking

### -O3: Aggressive Optimization

**All -O2 passes plus**:
- `loop-unroll` - Loop unrolling
- `vectorize` - Auto-vectorization (SIMD)
- `slp-vectorizer` - Superword-level parallelism
- `aggressive-instcombine` - Aggressive combining
- `tailcallelim` - Tail call elimination

**Characteristics**:
- Longest compile time
- Maximum performance (3-4x speedup)
- Larger binaries (loop unrolling, inlining)
- May use SIMD instructions

**Use Case**: Performance-critical code, final release

### -Os: Size Optimization

**Similar to -O2 but**:
- Lower inlining threshold (75)
- Size-focused heuristics
- No loop unrolling
- No vectorization

**Characteristics**:
- Smallest binary size
- ~2x speedup (less than O2)
- Good for embedded systems

**Use Case**: Resource-constrained environments

---

## Pass Descriptions

### Memory to Register (`mem2reg`)

**What**: Converts stack allocations to SSA registers

**Example**:
```llvm
# Before
%x = alloca i64
store i64 5, i64* %x
%val = load i64, i64* %x

# After
%val = i64 5
```

**Enabled**: O1+

### Instruction Combining (`instcombine`)

**What**: Combines multiple instructions into simpler forms

**Example**:
```llvm
# Before
%a = add i64 %x, 0
%b = mul i64 %a, 1

# After
%b = %x
```

**Enabled**: O1+

### Function Inlining (`inline`)

**What**: Replaces function calls with function body

**Example**:
```llvm
# Before
define i64 @add(i64 %a, i64 %b) {
  %sum = add i64 %a, %b
  ret i64 %sum
}

define i64 @main() {
  %result = call i64 @add(i64 5, i64 10)
  ret i64 %result
}

# After (inlined)
define i64 @main() {
  %result = add i64 5, 10
  ret i64 15  ; constant folded too
}
```

**Enabled**: O2+  
**Threshold**: O2=225, O3=275

### Global Value Numbering (`gvn`)

**What**: Common subexpression elimination

**Example**:
```llvm
# Before
%a = add i64 %x, %y
%b = add i64 %x, %y  ; Duplicate

# After
%a = add i64 %x, %y
%b = %a  ; Reuse
```

**Enabled**: O2+

### Loop-Invariant Code Motion (`licm`)

**What**: Moves loop-invariant computations outside loops

**Example**:
```llvm
# Before
for i in 0..1000:
  result = array[i] * (size + offset)  ; size+offset computed 1000 times

# After
temp = size + offset  ; Computed once
for i in 0..1000:
  result = array[i] * temp
```

**Enabled**: O2+

### Loop Unrolling (`loop-unroll`)

**What**: Duplicates loop body to reduce iteration overhead

**Example**:
```llvm
# Before
for i in 0..8:
  sum += array[i]

# After (unrolled by 4)
sum += array[0]
sum += array[1]
sum += array[2]
sum += array[3]
sum += array[4]
sum += array[5]
sum += array[6]
sum += array[7]
```

**Enabled**: O3  
**Tradeoff**: Larger code, fewer branches

### Auto-Vectorization (`vectorize`)

**What**: Converts scalar operations to SIMD vector operations

**Example**:
```llvm
# Before
for i in 0..16:
  c[i] = a[i] + b[i]  ; Scalar addition

# After (vectorized for SSE/AVX)
# Process 4 elements at once
for i in 0..16 step 4:
  c[i:i+4] = a[i:i+4] + b[i:i+4]  ; Vector addition
```

**Enabled**: O3  
**Requirements**: Target supports SIMD (SSE, AVX, NEON)

---

## Optimization Statistics

### Expected Performance

| Benchmark | Interpreter | -O0 | -O2 | -O3 | C -O3 |
|-----------|-------------|-----|-----|-----|-------|
| Fibonacci | 1000ms | 200ms | 80ms | 60ms | 40ms |
| Matrix Mult | 5000ms | 800ms | 300ms | 200ms | 120ms |
| Prime Sieve | 3000ms | 600ms | 250ms | 180ms | 100ms |

**Target**: -O3 within 1.5-2x of C

### Compile Time

| Level | Time for 1000 LOC | Factor |
|-------|-------------------|--------|
| -O0 | 0.5s | 1x |
| -O1 | 1.0s | 2x |
| -O2 | 2.0s | 4x |
| -O3 | 5.0s | 10x |

---

## Configuration

### Command-Line Flags

```bash
# Optimization levels
nlplc program.nlpl -O0  # No optimization
nlplc program.nlpl -O1  # Basic
nlplc program.nlpl -O2  # Standard (default)
nlplc program.nlpl -O3  # Aggressive
nlplc program.nlpl -Os  # Size

# Debug/analysis
nlplc program.nlpl --emit-llvm         # Output LLVM IR
nlplc program.nlpl --emit-llvm-opt     # Output optimized IR
nlplc program.nlpl --time              # Show compilation time
nlplc program.nlpl --verbose-opt       # Show applied passes
nlplc program.nlpl --no-opt            # Disable all optimization
```

### Programmatic API

```python
from nexuslang.compiler.llvm_optimizer import LLVMOptimizer, OptimizationLevel

# Create optimizer
optimizer = LLVMOptimizer(OptimizationLevel.O2)

# Optimize IR
optimized_ir = optimizer.optimize_module(llvm_ir_string)

# Get statistics
stats = optimizer.get_stats()
print(f"Functions optimized: {stats['functions_optimized']}")
print(f"Estimated speedup: {stats['estimated_speedup']}x")
```

---

## Testing

### Correctness Tests

All optimization levels must produce identical output:
```bash
# Test program
nlplc test.nlpl -O0 -o test_O0
nlplc test.nlpl -O2 -o test_O2
nlplc test.nlpl -O3 -o test_O3

# Verify output matches
./test_O0 > out_O0.txt
./test_O2 > out_O2.txt
./test_O3 > out_O3.txt

diff out_O0.txt out_O2.txt  # Must be identical
diff out_O0.txt out_O3.txt  # Must be identical
```

### Performance Tests

Measure speedup vs baseline:
```bash
# Benchmark
time ./test_O0  # Baseline
time ./test_O2  # Should be 2-3x faster
time ./test_O3  # Should be 3-4x faster
```

---

## Future Enhancements

### Phase 3 Week 2-3

- Custom optimization passes for NexusLang constructs
- Profile-guided optimization (PGO)
- Link-time optimization (LTO)
- Interprocedural analysis

### Phase 4+

- JIT compilation with LLVM ORC
- Adaptive optimization
- Feedback-directed optimization
- GPU code generation

---

## References

- **LLVM Optimization Passes**: https://llvm.org/docs/Passes.html
- **PassManager**: https://llvm.org/docs/WritingAnLLVMPass.html
- **llvmlite**: https://llvmlite.readthedocs.io/
- **NLPL Type System**: `docs/5_type_system/`
- **Compiler Architecture**: `docs/4_architecture/compiler_architecture.md`

---

## Appendix: Pass Ordering

LLVM's PassManager automatically orders passes for maximum effectiveness. Typical order:

1. **Module passes** (function-level first)
2. **Function passes** (within each function)
3. **Loop passes** (innermost loops first)
4. **Finalization** (cleanup, verification)

**Dependencies handled automatically** - e.g., `licm` requires `loop-simplify` to run first.

---

**Status**: Foundation complete, ready for Week 2 implementation.
