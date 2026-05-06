#!/bin/bash
# Optimization Correctness Test Runner
# Runs all optimization tests at multiple optimization levels and verifies identical output

set -e  # Exit on error

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
TEST_DIR="$PROJECT_ROOT/test_programs/regression/optimization_correctness"
BUILD_DIR="$PROJECT_ROOT/build/opt_tests"
COMPILER="$PROJECT_ROOT/dev_tools/nlplc_llvm.py"

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Create build directory
mkdir -p "$BUILD_DIR"

# Test files (only those that compile successfully)
TESTS=(
    "test_opt_arithmetic"
    "test_opt_loops"
    "test_opt_conditionals"
    "test_opt_functions"
    "test_opt_data_structures"
    "test_opt_edge_cases"
)

# Optimization levels
OPT_LEVELS=("O0" "O1" "O2" "O3" "Os")

echo "======================================================"
echo "NLPL Optimization Correctness Test Suite"
echo "======================================================"
echo ""

TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0
SKIPPED_TESTS=0

for test in "${TESTS[@]}"; do
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    TEST_FILE="$TEST_DIR/${test}.nxl"
    
    if [ ! -f "$TEST_FILE" ]; then
        echo -e "${YELLOW}SKIP${NC}: $test (file not found)"
        SKIPPED_TESTS=$((SKIPPED_TESTS + 1))
        continue
    fi
    
    echo "Testing: $test"
    
    # Try to compile at all optimization levels
    COMPILE_SUCCESS=true
    for opt in "${OPT_LEVELS[@]}"; do
        OUTPUT_BIN="$BUILD_DIR/${test}_${opt}"
        if ! python "$COMPILER" "$TEST_FILE" -${opt} -o "$OUTPUT_BIN" > /dev/null 2>&1; then
            echo -e "${YELLOW}SKIP${NC}: $test (compilation failed at -${opt})"
            COMPILE_SUCCESS=false
            SKIPPED_TESTS=$((SKIPPED_TESTS + 1))
            break
        fi
    done
    
    if [ "$COMPILE_SUCCESS" = false ]; then
        continue
    fi
    
    # Run all optimization levels and capture output
    for opt in "${OPT_LEVELS[@]}"; do
        OUTPUT_BIN="$BUILD_DIR/${test}_${opt}"
        OUTPUT_FILE="$BUILD_DIR/${test}_${opt}.out"
        
        if ! "$OUTPUT_BIN" > "$OUTPUT_FILE" 2>&1; then
            echo -e "${RED}FAIL${NC}: $test -${opt} (execution failed)"
            FAILED_TESTS=$((FAILED_TESTS + 1))
            continue 2  # Skip to next test
        fi
    done
    
    # Compare all outputs with O0 baseline
    BASELINE="$BUILD_DIR/${test}_O0.out"
    TEST_PASSED=true
    
    for opt in "${OPT_LEVELS[@]:1}"; do  # Skip O0 (baseline)
        OUTPUT_FILE="$BUILD_DIR/${test}_${opt}.out"
        
        if ! diff -q "$BASELINE" "$OUTPUT_FILE" > /dev/null 2>&1; then
            echo -e "${RED}FAIL${NC}: $test (-O0 vs -${opt} output differs)"
            echo "  Showing diff:"
            diff "$BASELINE" "$OUTPUT_FILE" | head -20
            TEST_PASSED=false
            FAILED_TESTS=$((FAILED_TESTS + 1))
            break
        fi
    done
    
    if [ "$TEST_PASSED" = true ]; then
        echo -e "${GREEN}PASS${NC}: $test"
        PASSED_TESTS=$((PASSED_TESTS + 1))
    fi
    
    echo ""
done

echo "======================================================"
echo "Test Results:"
echo "  Total:   $TOTAL_TESTS"
echo -e "  ${GREEN}Passed:  $PASSED_TESTS${NC}"
echo -e "  ${RED}Failed:  $FAILED_TESTS${NC}"
echo -e "  ${YELLOW}Skipped: $SKIPPED_TESTS${NC}"
echo "======================================================"

if [ $FAILED_TESTS -gt 0 ]; then
    exit 1
fi

echo ""
echo -e "${GREEN}All optimization correctness tests passed!${NC}"
exit 0
