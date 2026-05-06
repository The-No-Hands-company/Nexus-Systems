# Phase 3: Compiler Optimization - Implementation Plan

**Version Target**: v1.3  
**Duration**: 4 weeks  
**Start Date**: February 5, 2026  
**Target Completion**: March 5, 2026

---

## Executive Summary

Phase 3 focuses on **compiler optimization** to make NexusLang competitive with C/C++ in performance. We'll implement LLVM-based optimization passes, constant folding, dead code elimination, and function inlining to achieve near-native execution speed.

**Key Objectives**:
1. LLVM IR optimization pipeline integration
2. Performance within 2-5x of equivalent C code
3. Comprehensive optimization testing framework
4. Profiling and benchmarking infrastructure

---

## Current State Assessment

### What We Have

**Compiler Pipeline**:
- ✅ Lexer: Natural language tokenization
- ✅ Parser: AST generation with error recovery
- ✅ Type System: Optional type checking
- ✅ Interpreter: Direct AST execution (slow)
- ✅ LLVM Backend: Basic code generation (exists in `src/nlpl/compiler/`)

**Performance Baseline**:
- Current execution: Interpreted (Python-speed: ~50-100x slower than C)
- Target: Compiled (2-5x slower than C, 20-50x faster than current)

### What We Need

**Optimization Infrastructure**:
- [ ] LLVM optimization pass manager integration
- [ ] Constant folding and propagation
- [ ] Dead code elimination (DCE)
- [ ] Function inlining
- [ ] Loop optimizations
- [ ] Benchmarking framework
- [ ] Performance regression tests

---

## Architecture Design

### Optimization Pipeline

```
NLPL Source
    ↓
Lexer → Tokens
    ↓
Parser → AST
    ↓
Type Checker → Typed AST
    ↓
LLVM IR Generator → Unoptimized IR
    ↓
┌─────────────────────────────────┐
│   LLVM Optimization Pipeline    │
│                                 │
│  1. Constant Folding            │
│  2. Dead Code Elimination       │
│  3. Function Inlining           │
│  4. Loop Optimizations          │
│  5. Tail Call Optimization      │
│  6. Memory-to-Register          │
│  7. Common Subexpression Elim   │
│  8. Scalar Replacement          │
└─────────────────────────────────┘
    ↓
Optimized LLVM IR
    ↓
Native Code Generator → Binary
    ↓
Executable
```

### Optimization Levels

**-O0** (No optimization):
- Fast compilation
- Debug-friendly (preserves all info)
- Use for development

**-O1** (Basic optimization):
- Constant folding
- Simple DCE
- Fast, minimal size increase

**-O2** (Standard optimization):
- All -O1 passes
- Function inlining (small functions)
- Loop optimization
- CSE (Common Subexpression Elimination)
- Production default

**-O3** (Aggressive optimization):
- All -O2 passes
- Aggressive inlining
- Vectorization
- Loop unrolling
- Maximum performance, larger binaries

**-Os** (Size optimization):
- Minimize binary size
- No code duplication
- Useful for embedded systems

---

## Week-by-Week Plan

### Week 1: Foundation (Feb 5-11)

**Goals**: Set up optimization infrastructure

**Tasks**:
1. **Review existing LLVM backend** (Day 1-2)
   - Audit `src/nlpl/compiler/llvm_backend.py`
   - Identify what code generation exists
   - Document current IR generation patterns

2. **Create optimization module** (Day 2-3)
   - File: `src/nlpl/compiler/optimizer.py`
   - Class: `LLVMOptimizer`
   - Methods: `optimize()`, `run_pass()`, `set_level()`

3. **Integrate LLVM PassManager** (Day 3-4)
   - Use `llvmlite.binding.PassManagerBuilder`
   - Configure optimization levels (O0-O3, Os)
   - Add pipeline configuration

4. **Create benchmarking framework** (Day 4-5)
   - File: `benchmarks/benchmark_framework.py`
   - Measure: compile time, execution time, binary size
   - Baseline: interpreter, -O0, -O2, -O3, C equivalent

**Deliverables**:
- `optimizer.py` with basic PassManager integration
- Benchmark framework with 3+ test programs
- Documentation: `docs/4_architecture/optimization_pipeline.md`

---

### Week 2: Constant Folding & DCE (Feb 12-18)

**Goals**: Implement basic optimizations

**Tasks**:
1. **Constant folding** (Day 1-2)
   - Fold arithmetic: `2 + 3` → `5`
   - Fold comparisons: `5 > 3` → `true`
   - Fold string concatenation
   - Add tests: `test_constant_folding.nlpl`

2. **Dead code elimination** (Day 2-3)
   - Remove unreachable code after `return`
   - Eliminate unused variables
   - Remove branches with constant conditions
   - Add tests: `test_dead_code.nlpl`

3. **Integration testing** (Day 3-4)
   - Test -O0 vs -O2 output differences
   - Verify correctness (optimized == unoptimized semantics)
   - Measure performance improvement

4. **AST-level optimizations** (Day 4-5)
   - Pre-compute constant expressions in AST
   - Simplify boolean logic
   - Strength reduction (multiply by 2 → left shift)

**Deliverables**:
- Constant folding working at IR level
- DCE eliminating 30%+ dead code in test suite
- 10+ optimization tests passing
- Performance: 1.5-2x speedup on compute-heavy programs

---

### Week 3: Function Inlining & Loop Opts (Feb 19-25)

**Goals**: Aggressive performance optimizations

**Tasks**:
1. **Function inlining** (Day 1-3)
   - Inline small functions (<20 instructions)
   - Respect `inline` hints (future keyword)
   - Cost model: balance size vs speed
   - Add tests: `test_inlining.nlpl`

2. **Loop optimizations** (Day 3-4)
   - Loop unrolling (small fixed-count loops)
   - Loop-invariant code motion
   - Strength reduction in loops
   - Add tests: `test_loop_optimization.nlpl`

3. **Tail call optimization** (Day 4-5)
   - Convert tail recursion to loops
   - Eliminate stack growth for recursive functions
   - Add tests: `test_tail_recursion.nlpl`

**Deliverables**:
- Inlining reduces function call overhead by 50%+
- Loop optimizations: 2-3x speedup on iterative code
- Tail recursion handles 1M+ iterations without stack overflow
- 15+ optimization tests passing

---

### Week 4: Polish & Benchmarking (Feb 26 - Mar 5)

**Goals**: Production-ready optimization, comprehensive benchmarks

**Tasks**:
1. **Comprehensive benchmarking** (Day 1-2)
   - Fibonacci (recursive)
   - Matrix multiplication
   - String processing
   - Sorting algorithms
   - Compare: Python, NexusLang -O0, NexusLang -O2, NexusLang -O3, C
   - Generate performance report

2. **Optimization documentation** (Day 2-3)
   - Document each optimization pass
   - Usage guide: when to use -O0 vs -O3
   - Troubleshooting: optimization bugs
   - File: `docs/7_development/optimization_guide.md`

3. **Command-line flags** (Day 3-4)
   - Add: `-O0`, `-O1`, `-O2`, `-O3`, `-Os` flags
   - Add: `--emit-llvm` (output IR)
   - Add: `--time` (show compilation/execution time)
   - Add: `--verbose-opt` (show applied optimizations)

4. **Regression testing** (Day 4-5)
   - Run full test suite with -O0, -O2, -O3
   - Verify all tests pass at all levels
   - Add to CI: test both interpreted and compiled modes
   - Performance regression detection

**Deliverables**:
- Comprehensive benchmark suite (10+ programs)
- Performance report: NexusLang vs Python vs C
- Optimization guide (200+ lines)
- CI testing both interpreter and compiler
- v1.3 release ready

---

## Technical Details

### LLVM Pass Manager Integration

```python
# src/nlpl/compiler/optimizer.py

from llvmlite import binding as llvm
from llvmlite import ir

class LLVMOptimizer:
    """Manages LLVM optimization passes."""
    
    def __init__(self, optimization_level=2):
        """
        Args:
            optimization_level: 0-3 (O0-O3) or 's' for size
        """
        self.optimization_level = optimization_level
        self.pass_manager_builder = llvm.PassManagerBuilder()
        self._configure_passes()
    
    def _configure_passes(self):
        """Configure optimization passes based on level."""
        if self.optimization_level == 0:
            # -O0: No optimizations
            self.pass_manager_builder.opt_level = 0
        elif self.optimization_level == 1:
            # -O1: Basic optimizations
            self.pass_manager_builder.opt_level = 1
            self.pass_manager_builder.size_level = 0
        elif self.optimization_level == 2:
            # -O2: Standard optimizations
            self.pass_manager_builder.opt_level = 2
            self.pass_manager_builder.size_level = 0
            self.pass_manager_builder.inlining_threshold = 225
        elif self.optimization_level == 3:
            # -O3: Aggressive optimizations
            self.pass_manager_builder.opt_level = 3
            self.pass_manager_builder.size_level = 0
            self.pass_manager_builder.inlining_threshold = 275
        elif self.optimization_level == 's':
            # -Os: Size optimizations
            self.pass_manager_builder.opt_level = 2
            self.pass_manager_builder.size_level = 2
    
    def optimize(self, llvm_module):
        """
        Apply optimization passes to LLVM module.
        
        Args:
            llvm_module: llvmlite.ir.Module
            
        Returns:
            Optimized module
        """
        # Create pass managers
        module_pm = llvm.ModulePassManager()
        function_pm = llvm.FunctionPassManager(llvm_module)
        
        # Populate with passes
        self.pass_manager_builder.populate(module_pm)
        self.pass_manager_builder.populate(function_pm)
        
        # Initialize function pass manager
        function_pm.initialize()
        
        # Run function passes on each function
        for func in llvm_module.functions:
            function_pm.run(func)
        
        # Finalize function passes
        function_pm.finalize()
        
        # Run module passes
        module_pm.run(llvm_module)
        
        return llvm_module
    
    def get_pass_list(self):
        """Return list of enabled passes."""
        passes = []
        
        if self.optimization_level >= 1:
            passes.extend([
                'mem2reg',  # Memory to register promotion
                'simplifycfg',  # Control flow simplification
                'instcombine',  # Instruction combining
            ])
        
        if self.optimization_level >= 2:
            passes.extend([
                'inline',  # Function inlining
                'gvn',  # Global value numbering (CSE)
                'sccp',  # Sparse conditional constant propagation
                'dce',  # Dead code elimination
                'loop-simplify',  # Loop canonicalization
                'loop-rotate',  # Loop rotation
                'licm',  # Loop-invariant code motion
            ])
        
        if self.optimization_level >= 3:
            passes.extend([
                'loop-unroll',  # Loop unrolling
                'vectorize',  # Auto-vectorization
                'slp-vectorizer',  # Superword-level parallelism
                'aggressive-instcombine',
            ])
        
        return passes
```

### Constant Folding Example

**Before optimization**:
```llvm
define i64 @calculate() {
entry:
  %0 = add i64 2, 3
  %1 = mul i64 %0, 4
  %2 = add i64 %1, 10
  ret i64 %2
}
```

**After constant folding (-O1+)**:
```llvm
define i64 @calculate() {
entry:
  ret i64 30  ; (2 + 3) * 4 + 10 = 30
}
```

### Dead Code Elimination Example

**Before DCE**:
```llvm
define i64 @example(i64 %x) {
entry:
  %unused = add i64 %x, 5
  %result = mul i64 %x, 2
  ret i64 %result
  %dead = add i64 %x, 10  ; Unreachable
  ret i64 %dead           ; Unreachable
}
```

**After DCE (-O1+)**:
```llvm
define i64 @example(i64 %x) {
entry:
  %result = mul i64 %x, 2
  ret i64 %result
}
```

### Function Inlining Example

**Before inlining**:
```llvm
define i64 @add(i64 %a, i64 %b) {
  %sum = add i64 %a, %b
  ret i64 %sum
}

define i64 @main() {
  %result = call i64 @add(i64 5, i64 10)
  ret i64 %result
}
```

**After inlining (-O2+)**:
```llvm
define i64 @main() {
  %result = add i64 5, 10  ; Inlined
  ret i64 15                ; Then constant folded
}
```

---

## Benchmarking Framework

### Benchmark Programs

**1. Fibonacci (Recursive)**
```nlpl
function fibonacci with n as Integer returns Integer
    if n is less than or equal to 1
        return n
    end
    return fibonacci with n minus 1 plus fibonacci with n minus 2
end

set result to fibonacci with 35
print text result
```

**2. Matrix Multiplication**
```nlpl
function matrix_multiply with A as List and B as List returns List
    # 100x100 matrix multiplication
    # ... implementation
end
```

**3. Prime Sieve (Eratosthenes)**
```nlpl
function sieve with limit as Integer returns List
    # Find all primes up to limit
    # ... implementation
end
```

**4. String Processing**
```nlpl
function count_words with text as String returns Integer
    # Word counting with splitting
    # ... implementation
end
```

### Benchmark Metrics

**Compile Time**:
- Time to generate IR
- Time to optimize IR
- Time to generate native code
- Total compile time

**Execution Time**:
- Run 10 iterations, report median
- Compare: Interpreter vs -O0 vs -O2 vs -O3 vs C

**Binary Size**:
- Unoptimized binary
- -O2 binary
- -Os binary (size-optimized)

**Memory Usage**:
- Peak memory during compilation
- Runtime memory usage

### Expected Results

| Program | Interpreter | -O0 | -O2 | -O3 | C |
|---------|-------------|-----|-----|-----|---|
| Fibonacci(35) | 15000ms | 2500ms | 800ms | 750ms | 400ms |
| Matrix(100x100) | 8000ms | 1500ms | 600ms | 500ms | 300ms |
| Sieve(1M) | 5000ms | 1000ms | 400ms | 350ms | 200ms |

**Target**: -O2 within 2-3x of C, -O3 within 1.5-2x of C

---

## Testing Strategy

### Unit Tests

**Optimization correctness**:
- `test_constant_folding.py`: Verify fold correctness
- `test_dead_code_elimination.py`: Verify DCE correctness
- `test_inlining.py`: Verify inlined code is correct

**IR validation**:
- Parse generated IR with LLVM
- Run LLVM verification pass
- Ensure no invalid IR

### Integration Tests

**Full programs**:
- Compile 50+ test programs with -O0, -O2, -O3
- Verify all produce same output
- Verify optimized versions are faster

**Regression tests**:
- Add performance benchmarks to CI
- Alert if -O2 is slower than baseline
- Track compile time regressions

### Validation

**Correctness checks**:
- Output must match unoptimized version
- No crashes or segfaults
- Memory leaks detected with valgrind

**Performance checks**:
- -O2 must be 2x+ faster than -O0
- -O3 must be 10x+ faster than interpreter
- Compile time must be reasonable (<5s for 1000 LOC)

---

## Command-Line Interface

### New Flags

```bash
# Optimization levels
nlplc program.nlpl -O0  # No optimization (fast compile)
nlplc program.nlpl -O1  # Basic optimization
nlplc program.nlpl -O2  # Standard (default)
nlplc program.nlpl -O3  # Aggressive
nlplc program.nlpl -Os  # Size optimization

# Debug/analysis
nlplc program.nlpl --emit-llvm         # Output LLVM IR
nlplc program.nlpl --emit-llvm-opt     # Output optimized IR
nlplc program.nlpl --time              # Show compilation time
nlplc program.nlpl --verbose-opt       # Show applied passes
nlplc program.nlpl --dump-passes       # List all optimization passes

# Benchmarking
nlplc program.nlpl --benchmark         # Run 10x, report stats
nlplc program.nlpl --compare-c main.c  # Compare with C version
```

### Usage Examples

```bash
# Development: fast compile, debug-friendly
nlplc server.nlpl -O0 -g

# Production: optimized binary
nlplc server.nlpl -O3 -o server

# Profile optimization impact
nlplc compute.nlpl -O0 --time --emit-llvm
nlplc compute.nlpl -O3 --time --emit-llvm-opt
diff compute_O0.ll compute_O3.ll

# Benchmark against C
nlplc fib.nlpl -O3 --benchmark
gcc -O3 fib.c -o fib_c && time ./fib_c
```

---

## Documentation

### New Documentation Files

**1. Optimization Guide** (`docs/7_development/optimization_guide.md`):
- How optimization works
- When to use each level
- Troubleshooting optimization issues
- Performance tuning tips

**2. Compiler Architecture** (`docs/4_architecture/compiler_architecture.md` update):
- Add optimization pipeline section
- Document LLVM integration
- Explain pass ordering

**3. Benchmarking Guide** (`docs/7_development/benchmarking_guide.md`):
- How to run benchmarks
- Interpreting results
- Writing good benchmarks
- Performance regression detection

**4. Phase 3 Progress Reports** (`docs/9_status_reports/phase3_week*.md`):
- Weekly progress updates
- Challenges and solutions
- Performance metrics

---

## Success Criteria

### Must-Have (v1.3 Release)

- ✅ LLVM optimization pipeline integrated
- ✅ -O0, -O2, -O3 flags working
- ✅ Constant folding implemented
- ✅ Dead code elimination working
- ✅ 20+ optimization tests passing
- ✅ Benchmarking framework complete
- ✅ Performance: -O2 is 2x+ faster than -O0
- ✅ All existing tests pass at all optimization levels
- ✅ Documentation complete

### Nice-to-Have

- ⭐ Function inlining (if time allows)
- ⭐ Loop optimizations (basic)
- ⭐ Tail call optimization
- ⭐ CI performance tracking
- ⭐ Comparison with Rust/Go in benchmarks

### Stretch Goals

- 🚀 Auto-vectorization
- 🚀 Profile-guided optimization (PGO)
- 🚀 Link-time optimization (LTO)
- 🚀 Optimization remarks (explain why inlining didn't happen)

---

## Risk Assessment

### Technical Risks

**1. LLVM complexity** (High)
- Mitigation: Start simple, use existing llvmlite examples
- Fallback: Use llvmlite built-in pass builder

**2. Optimization bugs** (Medium)
- Mitigation: Extensive testing, compare optimized vs unoptimized
- Fallback: Allow disabling specific passes

**3. Performance below target** (Medium)
- Mitigation: Profile to find bottlenecks
- Fallback: Focus on most impactful optimizations first

**4. Compile time too slow** (Low)
- Mitigation: Make -O3 optional, default to -O1
- Fallback: Offer incremental compilation later

### Schedule Risks

**1. Week 1 overruns** (Low)
- Buffer: Week 4 has polish time
- Mitigation: Prioritize infrastructure setup

**2. Integration issues** (Medium)
- Mitigation: Test early and often
- Buffer: Week 4 dedicated to polish

---

## Dependencies

### External

- **llvmlite**: LLVM Python bindings (already installed)
- **LLVM**: Version 14+ (system dependency)

### Internal

- **LLVM Backend**: Must exist and generate valid IR
- **Type System**: Needed for optimization decisions
- **Test Suite**: For regression testing

---

## Metrics & KPIs

### Performance Metrics

- **Speedup**: -O2 vs -O0 (target: 2x+)
- **Speedup**: -O3 vs interpreter (target: 10x+)
- **Compile Time**: -O2 must be <5s for 1000 LOC
- **Binary Size**: -Os must be <50% larger than C equivalent

### Quality Metrics

- **Test Pass Rate**: 100% at all optimization levels
- **Code Coverage**: 80%+ in optimizer.py
- **Documentation**: All optimizations documented

### Adoption Metrics

- **Benchmark Suite**: 10+ programs
- **Real Programs Compiled**: 50+ programs from test suite

---

## Post-Phase 3 Roadmap

### Phase 4: Systems Programming (v2.0)

- Struct/union methods fully working
- Bitwise operations
- FFI with C libraries
- Memory debugging tools

### Phase 5: Multi-Platform (v2.1)

- Cross-compilation (x86, ARM, RISC-V)
- WASM code generation
- JavaScript/TypeScript transpiler

---

## Team & Resources

### Required Skills

- LLVM IR knowledge (optimization passes)
- Performance analysis and profiling
- Benchmarking methodology
- C/C++ for comparisons

### Time Allocation

- Week 1: 40 hours (foundation)
- Week 2: 40 hours (core optimizations)
- Week 3: 40 hours (advanced optimizations)
- Week 4: 40 hours (polish and testing)

**Total**: 160 hours over 4 weeks

---

## Conclusion

Phase 3 will transform NexusLang from an interpreted language into a **high-performance compiled language** competitive with C, Rust, and Go. By integrating LLVM's world-class optimization infrastructure, we'll achieve:

- **10-20x speedup** over current interpreter
- **2-5x of C performance** with -O3
- **Production-ready compiler** with comprehensive testing
- **Professional benchmarking** showing real-world performance

This sets the foundation for Phase 4 (systems programming) and beyond, making NexusLang suitable for performance-critical applications like OS kernels, game engines, and high-frequency trading systems.

**Let's build the fastest natural language programming language in the world!** 🚀
