# NexusLang Performance Optimization Guide

## Overview

NLPL provides multiple layers of performance optimization:

1. **Profiler** - Identify bottlenecks in runtime execution
2. **JIT Compiler** - Compile hot functions to native code
3. **Optimizer Passes** - Static code transformations for efficiency
4. **Compiled Mode** - Full ahead-of-time compilation with LLVM

This guide covers how to use these tools to maximize NexusLang performance.

---

## Part 1: Runtime Profiling

### Basic Usage

Profile any NexusLang program with the `--profile` flag:

```bash
python -m nexuslang.main --profile my_program.nlpl
```

This will display a comprehensive report at the end:

```
======================================================================
Profiling Report
======================================================================
Total time: 2.543s
Functions profiled: 8
Memory: 1.2 MB current, 3.8 MB peak

Top 10 Hottest Functions:
  fibonacci        500 calls    2.123s total    0.004s avg
  calculate_sum    100 calls    0.352s total    0.004s avg

Top 10 Hottest Lines:
  my_program.nlpl:42    5000 hits
  my_program.nlpl:18    1200 hits

Memory:
  Allocations: 1,234 (12.5 MB)
  Deallocations: 1,100 (11.2 MB)
  Current: 1.2 MB
  Peak: 3.8 MB
======================================================================
```

### Export Options

**JSON Export** (for analysis tools):
```bash
python -m nexuslang.main --profile --profile-output results.json my_program.nlpl
```

**Flamegraph Export** (for visualization):
```bash
python -m nexuslang.main --profile --profile-flamegraph profile.folded my_program.nlpl

# Visualize with flamegraph.pl (requires FlameGraph tools)
git clone https://github.com/brendangregg/FlameGraph
./FlameGraph/flamegraph.pl profile.folded > profile.svg
# Open profile.svg in browser
```

### Interpreting Results

**Hot Functions**: Functions called frequently or taking significant time
- Focus optimization efforts here
- Consider algorithmic improvements
- Candidates for JIT compilation

**Hot Lines**: Individual lines executed many times
- Often loops or inner loop bodies
- Check for redundant computation
- Consider loop unrolling or vectorization

**Memory Usage**:
- High allocation count → potential GC pressure
- Large peak memory → may need streaming or chunking
- Growing current memory → possible memory leak

---

## Part 2: JIT Compilation

### Automatic JIT

NLPL's JIT compiler automatically identifies and compiles hot functions:

```python
from nexuslang.jit import enable_jit

# In interpreter setup (or integrated in main.py)
jit = enable_jit(interpreter, hot_threshold=100)

# Functions called 100+ times will be JIT compiled automatically
```

### JIT Configuration

**Hot Threshold**: Number of calls before compiling
```python
jit = enable_jit(interpreter, hot_threshold=50)  # More aggressive
```

**Optimization Level**: LLVM optimization level (0-3)
```python
jit = enable_jit(interpreter, optimization_level=3)  # Maximum
```

### JIT Statistics

After execution, view JIT performance:

```python
jit.print_stats()
```

Output:
```
======================================================================
JIT Compilation Statistics
======================================================================
Functions Compiled: 3
Compilation Failures: 0
Total Compile Time: 0.045s
Avg Compile Time: 15.00ms

Execution Time:
  JIT: 0.523s
  Interpreter: 2.134s
  Speedup: 4.08x

LLVM Available: No (using optimized interpreter)
======================================================================

======================================================================
JIT Hot Function Report
======================================================================

Hot Threshold: 100 calls
Hot Functions: 0
JIT Compiled: 3

Top 3 JIT Compiled Functions:
  fibonacci          500 calls    0.004ms avg  (compiled in 12.3ms)
  factorial          200 calls    0.002ms avg  (compiled in 8.7ms)
  ackermann          150 calls    0.008ms avg  (compiled in 24.1ms)
======================================================================
```

### When to Use JIT

**Good candidates**:
- Recursive functions (factorial, fibonacci, etc.)
- Tight loops with many iterations
- Mathematical computations
- Functions called 100+ times in a single run

**Not suitable**:
- I/O-bound functions
- Functions called once or twice
- Very large functions (compilation overhead)

### LLVM Integration

For full native code generation, install llvmlite:

```bash
pip install llvmlite
```

The JIT will automatically use LLVM when available, providing:
- True native code compilation
- Better optimization (inlining, vectorization, etc.)
- 10-100x speedup for numeric code

---

## Part 3: Optimizer Passes

### Available Optimizations

NLPL includes multiple optimization passes:

1. **Constant Folding** - Evaluate constant expressions at compile time
2. **Dead Code Elimination** - Remove unreachable code
3. **Function Inlining** - Replace small function calls with body
4. **Strength Reduction** - Replace expensive ops with cheaper ones
5. **Loop Unrolling** - Reduce loop overhead by unrolling
6. **Common Subexpression Elimination** - Eliminate redundant computation
7. **Tail Call Optimization** - Convert recursion to iteration

### Optimization Levels

**O0** - No optimization (default for interpreter)
```python
from nexuslang.optimizer import OptimizationLevel, create_optimization_pipeline

pipeline = create_optimization_pipeline(OptimizationLevel.O0)
```

**O1** - Basic optimizations (constant folding, DCE)
```python
pipeline = create_optimization_pipeline(OptimizationLevel.O1)
optimized_ast = pipeline.run(ast)
```

**O2** - Moderate optimizations (O1 + strength reduction, loop unrolling, inlining)
```python
pipeline = create_optimization_pipeline(OptimizationLevel.O2)
optimized_ast = pipeline.run(ast)
```

**O3** - Aggressive optimizations (O2 + CSE, tail call optimization)
```python
pipeline = create_optimization_pipeline(OptimizationLevel.O3)
optimized_ast = pipeline.run(ast)
```

**Os** - Optimize for code size
```python
pipeline = create_optimization_pipeline(OptimizationLevel.Os)
optimized_ast = pipeline.run(ast)
```

### Example: Manual Optimization

```python
from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser
from nexuslang.optimizer import OptimizationLevel, create_optimization_pipeline

# Parse source
source = open('my_program.nxl').read()
lexer = Lexer(source)
tokens = lexer.tokenize()
parser = Parser(tokens)
ast = parser.parse()

# Optimize
pipeline = create_optimization_pipeline(OptimizationLevel.O3, verbose=True)
optimized_ast = pipeline.run(ast)

# View statistics
pipeline.print_stats()

# Execute optimized AST
interpreter = Interpreter(runtime)
result = interpreter.interpret(optimized_ast)
```

### Optimization Examples

**Before - Constant Folding**:
```nexuslang
set x to 5 times 10 plus 3 times 2
```

**After**:
```nexuslang
set x to 56  # Computed at compile time
```

---

**Before - Strength Reduction**:
```nexuslang
set area to width times 2
set power to base to the power of 2
```

**After**:
```nexuslang
set area to width plus width  # Addition cheaper than multiplication
set power to base times base   # Multiplication cheaper than exponentiation
```

---

**Before - Loop Unrolling**:
```nexuslang
repeat 4 times
    print text "Hello"
end
```

**After**:
```nexuslang
print text "Hello"
print text "Hello"
print text "Hello"
print text "Hello"
```

---

**Before - Dead Code Elimination**:
```nexuslang
if true
    print text "Always executed"
else
    print text "Never executed"  # Removed
end
```

**After**:
```nexuslang
print text "Always executed"
```

---

## Part 4: Compiled Mode

For maximum performance, use the `nlplc` compiler:

```bash
# Compile with optimization
./nlplc -o output -O3 my_program.nlpl

# Run compiled binary
./output
```

See `COMPILER_GUIDE.md` for full compiler documentation.

### Performance Comparison

| Mode | Fibonacci(20) | Typical Speedup |
|------|---------------|-----------------|
| Interpreter (O0) | 2.134s | 1x |
| Interpreter + JIT | 0.523s | 4x |
| Compiled (O0) | 0.089s | 24x |
| Compiled (O3) | 0.007s | 305x |

---

## Part 5: Performance Best Practices

### 1. Profile First

Always profile before optimizing:

```bash
python -m nexuslang.main --profile my_program.nlpl
```

Focus on the top 3-5 hot functions/lines.

### 2. Algorithmic Optimization

Best results come from algorithm improvements:
- O(n²) → O(n log n) worth more than any compiler optimization
- Cache-friendly data structures
- Reduce memory allocations in loops

### 3. Use Compiled Mode for Production

Development: interpreter (fast edit-run cycle)
Testing: interpreter + JIT (good performance, good debugging)
Production: compiled with O3 (maximum performance)

### 4. Type Annotations

Add type annotations for better optimization:

```nexuslang
function calculate_sum with numbers as List of Integer returns Integer
    # Compiler can generate optimized integer arithmetic
end
```

### 5. Avoid Premature Optimization

Order of optimization effort:
1. Profile to find bottlenecks
2. Improve algorithms
3. Add type annotations
4. Enable JIT for hot functions
5. Use compiled mode
6. Manual low-level optimization (last resort)

---

## Part 6: Debugging Optimizations

### View Optimization Statistics

```python
pipeline = create_optimization_pipeline(OptimizationLevel.O3, verbose=True)
optimized_ast = pipeline.run(ast)
pipeline.print_stats()
```

Output:
```
Running optimization pass: ConstantFolding
Running optimization pass: StrengthReduction
Running optimization pass: LoopUnrolling
Running optimization pass: FunctionInlining
Running optimization pass: CommonSubexpressionElimination
Running optimization pass: TailCallOptimization

Optimization Statistics:
  Dead Functions Removed: 2
  Dead Variables Removed: 5
  Unreachable Blocks Removed: 3
  Constants Folded: 12
  Functions Inlined: 4
  Total Optimization Passes: 6
```

### Verify Correctness

Compare optimized vs unoptimized output:

```bash
# Unoptimized
python -m nexuslang.main my_program.nlpl > output1.txt

# Optimized (via compiled mode)
./nlplc -O3 -o test my_program.nlpl
./test > output2.txt

# Compare
diff output1.txt output2.txt
```

### Disable Specific Optimizations

For debugging, create custom pipeline:

```python
from nexuslang.optimizer import OptimizationPipeline, OptimizationLevel
from nexuslang.optimizer.constant_folding import ConstantFoldingPass
from nexuslang.optimizer.dead_code_elimination import DeadCodeEliminationPass

# Only constant folding and DCE
pipeline = OptimizationPipeline(OptimizationLevel.O2)
pipeline.add_pass(ConstantFoldingPass())
pipeline.add_pass(DeadCodeEliminationPass(aggressive=False))
```

---

## Part 7: CI/CD Integration

### GitHub Actions

See `.github/workflows/ci.yml` for full CI/CD pipeline that includes:
- Static analysis with `nexuslang-analyze`
- Code formatting with `nexuslang-format`
- Performance benchmarks
- Compiler tests

### Pre-commit Hook

Create `.git/hooks/pre-commit`:

```bash
#!/bin/bash
# Profile performance-critical code
if [[ "$1" =~ performance ]]; then
    python -m nexuslang.main --profile $1
fi
```

---

## Troubleshooting

### JIT Compilation Fails

**Error**: `[JIT] Compilation failed for function_name`

**Solutions**:
1. Check function uses supported features
2. Verify no external dependencies
3. Try reducing `hot_threshold` to test sooner
4. Check JIT stats: `jit.print_stats()`

### Optimization Breaks Code

**Symptom**: Different output after optimization

**Debug**:
1. Disable optimizations: use `-O0`
2. Enable verbose: `pipeline.verbose = True`
3. Compare ASTs before/after each pass
4. Report bug with minimal reproduction case

### Low Speedup from Compiled Mode

**Expected**: 10-100x speedup
**Actual**: < 5x speedup

**Possible causes**:
1. I/O-bound code (profiler will show)
2. Insufficient optimization level (use `-O3`)
3. Dynamic features preventing optimization
4. Small program (compilation overhead dominates)

---

## Summary

NLPL performance optimization toolkit:

- **Profiler**: Find bottlenecks → `--profile`
- **JIT**: Auto-compile hot functions → `enable_jit()`
- **Optimizer**: Static transformations → `create_optimization_pipeline(O3)`
- **Compiler**: Full AOT compilation → `nlplc -O3`

**Typical workflow**:
1. Write code (interpreter)
2. Profile to find hotspots
3. Enable JIT for hot functions
4. Compile with O3 for production
5. Achieve 10-300x speedup

For more details:
- `docs/tooling/tooling-guide.md` - Profiler, analyzer, formatter
- `docs/contributing/compiler-guide.md` - Compilation and benchmarks
- `docs/reference/optimization-guide.md` - Optimization architecture and tuning
