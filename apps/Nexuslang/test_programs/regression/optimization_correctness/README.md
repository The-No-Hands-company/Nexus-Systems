# Optimization Correctness Test Suite

This directory contains test programs that verify LLVM optimization passes preserve program semantics. Each test should produce **identical output** regardless of optimization level (-O0, -O1, -O2, -O3, -Os).

## Test Categories

### 1. Arithmetic Operations (`test_opt_arithmetic.nlpl`)
Tests that arithmetic optimizations preserve:
- Integer arithmetic (addition, subtraction, multiplication, division)
- Negative numbers
- Modulo operations
- Operator precedence
- Division edge cases

**Expected Results:**
- Arithmetic test: 1000000
- Negatives test: -50
- Modulo test: 1683
- Precedence test: 20
- Division test: 33

### 2. Loop Optimizations (`test_opt_loops.nlpl`)
Tests that loop transformations preserve:
- Simple counting loops
- Nested loops
- Loops with early exit
- Loops with conditional logic
- Accumulator patterns

**Expected Results:**
- Simple loop: 4950
- Nested loops: 100
- Loop break: 50
- Loop skip: 50
- Accumulator: 3628800

### 3. Conditional Optimizations (`test_opt_conditionals.nlpl`)
Tests that branch optimizations preserve:
- Simple if-else statements
- Nested conditionals
- Logical operators (and, or)
- Equality checks
- Complex boolean expressions

**Expected Results:**
- Simple conditional (10): 1
- Simple conditional (-5): -1
- Nested conditional (250): 3
- Nested conditional (150): 2
- Nested conditional (75): 1
- Logical operators (5, 10): 1
- Logical operators (-5, 10): -1
- Equality (42): 1000
- Complex boolean (10, 5, 3): 1

### 4. Function Call Optimizations (`test_opt_functions.nlpl`)
Tests that inlining and call optimizations preserve:
- Recursive functions (factorial)
- Mutually recursive functions (is_even/is_odd)
- Helper functions (potential inlining candidates)
- Fibonacci (deep recursion)
- Functions with side effects

**Expected Results:**
- Factorial(5): 120
- Factorial(10): 3628800
- Is_even(10): 1
- Is_odd(10): 0
- Inlining test: 50
- Fibonacci(10): 55
- Side effects (15): 30

### 5. Data Structure Optimizations (`test_opt_data_structures.nlpl`)
Tests that optimizations preserve:
- List operations
- List building in loops
- Dictionary operations
- Nested data structures
- List modification (index assignment)

**Expected Results:**
- List operations: 150
- List building: 45
- Dict operations: 274
- Nested structures: 36
- List modification: 69

### 6. Edge Cases (`test_opt_edge_cases.nlpl`)
Tests boundary conditions:
- Zero handling
- Boundary values (INT_MAX, INT_MIN)
- Identity operations (multiply by 1, divide by 1, add 0)
- Boolean edge cases
- Empty collections
- Comparison chains

**Expected Results:**
- Zero operations: 200
- Boundaries: 2
- Identity operations: 42
- Boolean edges (0): 0
- Boolean edges (1): 1
- Empty collections: 0
- Comparison chains: 2

## Running Tests

### Manual Testing

Run each test at all optimization levels and verify identical output:

```bash
# Compile at different optimization levels
python dev_tools/nlplc_llvm.py test_opt_arithmetic.nlpl -O0 -o test_o0
python dev_tools/nlplc_llvm.py test_opt_arithmetic.nlpl -O1 -o test_o1
python dev_tools/nlplc_llvm.py test_opt_arithmetic.nlpl -O2 -o test_o2
python dev_tools/nlplc_llvm.py test_opt_arithmetic.nlpl -O3 -o test_o3
python dev_tools/nlplc_llvm.py test_opt_arithmetic.nlpl -Os -o test_os

# Run and compare output
./test_o0 > output_o0.txt
./test_o1 > output_o1.txt
./test_o2 > output_o2.txt
./test_o3 > output_o3.txt
./test_os > output_os.txt

# Verify all outputs are identical
diff output_o0.txt output_o1.txt
diff output_o0.txt output_o2.txt
diff output_o0.txt output_o3.txt
diff output_o0.txt output_os.txt
```

### Automated Testing Script

```bash
#!/bin/bash
# run_optimization_tests.sh

TESTS="test_opt_arithmetic test_opt_loops test_opt_conditionals test_opt_functions test_opt_data_structures test_opt_edge_cases"
OPT_LEVELS="O0 O1 O2 O3 Os"

for test in $TESTS; do
    echo "Testing $test..."
    
    # Compile at all levels
    for opt in $OPT_LEVELS; do
        python dev_tools/nlplc_llvm.py test_programs/regression/optimization_correctness/${test}.nlpl -${opt} -o build/${test}_${opt}
        build/${test}_${opt} > build/${test}_${opt}.out
    done
    
    # Compare outputs
    baseline=build/${test}_O0.out
    for opt in O1 O2 O3 Os; do
        if ! diff -q $baseline build/${test}_${opt}.out > /dev/null; then
            echo "FAILED: $test -${opt} produces different output!"
            diff $baseline build/${test}_${opt}.out
            exit 1
        fi
    done
    
    echo "PASSED: $test"
done

echo ""
echo "All optimization correctness tests passed!"
```

## Expected Behavior

**All tests should:**
- Compile successfully at all optimization levels
- Produce identical output regardless of optimization level
- Complete without crashes or errors
- Execute in reasonable time (< 1 second per test)

**If a test fails:**
1. Check the diff output to see what changed
2. Examine the optimized IR (`--ir-opt` flag) to see what transformation caused the issue
3. File a bug report with test case and IR

## Adding New Tests

When adding new optimization correctness tests:

1. **Pick a specific optimization** (e.g., constant folding, loop unrolling)
2. **Create minimal reproduction** that exercises that optimization
3. **Include expected output** in comments
4. **Test all edge cases** (zero, negative, overflow, empty)
5. **Verify manually** at all optimization levels before committing

## Test Maintenance

- Run this suite **before every release**
- Run after **any change to llvm_optimizer.py or llvm_ir_generator.py**
- Add new tests when **bugs are found** (regression tests)
- Keep tests **fast** (< 1 second each)

## Current Status

**Total Tests:** 6  
**Categories:** Arithmetic, Loops, Conditionals, Functions, Data Structures, Edge Cases  
**Coverage:** ~80% of common optimization patterns  

**Known Gaps:**
- String optimizations (when string type is fully implemented)
- Floating-point optimizations (when float type is added)
- Memory optimizations (pointer aliasing, etc.)
- Async/await optimizations (coroutine passes)
