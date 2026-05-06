# Test Programs Directory

This directory contains **NLPL test programs** used for testing the NexusLang language implementation. These are distinct from the Python unit tests in `tests/` and the tutorial examples in `examples/`.

## Purpose

**test_programs/** contains NexusLang source files (`.nlpl`) that validate language behavior through execution. These are used for:
- Integration testing (multi-feature validation)
- Regression testing (bug reproduction and fixes)
- Unit testing individual language features
- Performance benchmarking
- Edge case validation

## Directory Structure

### `unit/` - Single Feature Tests
Tests that focus on **one specific language feature** in isolation.

**Subdirectories:**
- `basic/` - Core basics (hello world, variables, print)
- `control_flow/` - If/else, loops, switch statements
- `functions/` - Function definitions, parameters, returns
- `generics/` - Generic types and functions
- `modules/` - Module imports and namespaces
- `oop/` - Object-oriented programming basics
- `parser/` - Parser-specific edge cases
- `stdlib/` - Standard library function tests

**Example:** `unit/basic/test_hello.nlpl` - Simple hello world test
**Example:** `unit/stdlib/test_math.nlpl` - Math module functions

### `integration/` - Multi-Feature Tests
Tests that combine **multiple language features** and validate complex interactions.

**Subdirectories:**
- `classes/` - Complete class implementations with methods, inheritance
- `compiler/` - Full compilation pipeline tests (173 files)
- `debug/` - Debugging features and tools
- `features/` - Advanced features (pattern matching, Option/Result types, etc.)
- `ffi/` - Foreign Function Interface (C interop)
- `foundation/` - Low-level features (pointers, memory, bitwise ops)
- `optimization/` - Performance optimization tests
- `static_analysis/` - Static analysis and type checking

**Example:** `integration/compiler/comprehensive_test.nlpl` - Full language feature showcase
**Example:** `integration/features/test_pattern_matching.nlpl` - Pattern matching with multiple types

### `regression/` - Bug Fix Validation
Tests that reproduce **previously found bugs** to prevent regressions.

**Subdirectories:**
- `error_handling/` - Error handling and recovery
- `error_tests/` - Syntax and type errors

**Example:** `regression/test_index_error.nlpl` - Index out of bounds error handling
**Example:** `regression/test_name_error.nlpl` - Undefined variable suggestions

## How to Run Tests

### Run Individual Test
```bash
python src/main.py test_programs/unit/basic/test_hello.nlpl
python src/main.py test_programs/integration/compiler/comprehensive_test.nlpl
```

### Run with Debug Output
```bash
python src/main.py test_programs/unit/control_flow/test_if_else.nlpl --debug
```

### Run All Tests in a Category
```bash
find test_programs/unit/basic/ -name "*.nxl" -exec python src/main.py {} \;
find test_programs/integration/features/ -name "*.nxl" -exec python src/main.py {} \;
```

## Difference from Other Directories

### test_programs/ vs examples/
- **test_programs/**: Test validation files - may test edge cases, errors, specific features
- **examples/**: Tutorial documentation - clean, numbered, educational programs

### test_programs/ vs tests/
- **test_programs/**: NexusLang source files executed by the interpreter (tests the **language**)
- **tests/**: Python unit tests using pytest (tests the **implementation**)

## File Statistics

- **Total NexusLang files**: 339
- **Unit tests**: ~120 files (single feature validation)
- **Integration tests**: ~210 files (multi-feature validation)
- **Regression tests**: ~9 files (bug fixes)

## Contributing Guidelines

When adding new test programs:

1. **Determine category**: Is this testing one feature (unit), multiple features (integration), or a bug fix (regression)?

2. **Choose subdirectory**: Place in appropriate subdirectory based on feature domain

3. **Naming convention**: 
   - Unit tests: `test_<feature>.nlpl`
   - Integration tests: `test_<scenario>_<features>.nlpl`
   - Regression tests: `test_<bug_description>.nlpl`

4. **Add comments**: Include description of what's being tested at the top of the file

5. **Keep focused**: Each test should have a clear, specific purpose

## Test Organization Philosophy

This structure follows industry-standard testing practices:

- **Unit tests** are fast, isolated, and test individual components
- **Integration tests** are comprehensive and test feature interactions
- **Regression tests** prevent previously fixed bugs from reappearing

Similar to how languages like Python, Rust, and Go organize their test suites.
