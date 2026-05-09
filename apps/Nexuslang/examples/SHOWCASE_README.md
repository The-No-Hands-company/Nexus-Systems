# Showcase: Full Development Workflow

This showcase demonstrates the complete NexusLang development workflow, from static analysis through execution.

## Hardened Capability Demo Project

For a reproducible, multi-module end-to-end workflow demo with fixtures and snapshots, use:

```text
examples/showcase_workflow/
```

This project includes:
- Multi-module source under `src/`
- Reproducible input fixtures under `fixtures/`
- Output and benchmark snapshots under `snapshots/`
- Pipeline scripts for lint/check/build/run/profile/sanitizer under `scripts/`

Run the complete pipeline from repository root:

```bash
bash examples/showcase_workflow/scripts/run_pipeline.sh
```

Generate benchmark snapshots:

```bash
bash examples/showcase_workflow/scripts/benchmark_snapshot.sh
```

## Overview

The `showcase_workflow_demo.nxl` program demonstrates:

- **Static Linting**: Static analyzer identifies potential issues
  - Type mismatches
  - Unused variables
  - Resource leaks
  - Memory safety violations

- **Type Checking**: Optional strong typing validation
  - Function argument types
  - Variable declaration types
  - Return type verification

- **Compilation**: Build the program to native code
  - Lexing & parsing
  - AST generation
  - Backend code generation (C or LLVM)
  - Native compilation

- **Execution**: Run the compiled program

## Running the Workflow

### Step 1: Static Analysis

Check for potential issues without compilation:

```bash
nlpl lint examples/showcase_workflow_demo.nxl
```

This will report:
- Type mismatches (if strong typing enabled)
- Unused variables
- Potential memory issues
- Code style issues

### Step 2: Build

Compile the program to an executable:

```bash
nlpl build --release
```

Or with sanitizers enabled for runtime checking:

```bash
nlpl build --sanitize address thread undefined
```

### Step 2b: Build + Run With Runtime Analysis

Generate combined sanitizer + profiling artifacts for the same workflow:

```bash
nlpl run --sanitize address --analyze-runtime
```

Artifacts are written to:

```text
build/runtime-analysis/
```

### Step 3: Run

Execute the compiled program:

```bash
nlpl run
```

### Complete Workflow (One Command)

Build and run in one step:

```bash
nlpl build --release && nlpl run
```

## Sanitizer Integration

The build system supports multiple runtime sanitizers:

- **Address Sanitizer** (`--sanitize address`): Detects memory errors
- **Thread Sanitizer** (`--sanitize thread`): Detects data races
- **Memory Sanitizer** (`--sanitize memory`): Detects uninitialized reads
- **UB Sanitizer** (`--sanitize undefined`): Detects undefined behavior

Combine multiple sanitizers:

```bash
nlpl build --sanitize address thread memory undefined
```

Or enable all at once:

```bash
nlpl build --sanitize all
```

## Program Features

The showcase program includes:

### Data Processing Pipeline
- Function definitions with type annotations
- List and Map data structures
- Loop iteration and data transformation
- String processing

### Error Handling
- Type validation
- Empty result handling
- Error reporting

### Multi-Stage Processing
1. Data preparation
2. Processing pipeline
3. Analysis and aggregation
4. Result reporting

## Expected Output

When run successfully, the program outputs:

```
NexusLang Development Workflow Showcase
========================================

Stage 1: Preparing data...
example,data,input

Stage 2: Processing data...
example,data,input

Stage 3: Analyzing results...
Analysis complete

Workflow completed successfully
```

## Lint Issues

The program intentionally includes several lint issues for demonstration:

1. **Unused Variable** (line 24): `ignored_value` is never used
2. **Type Mismatch** (line 21): String to untyped variable assignment
3. **Potential Resource Leak** (line 43): Map created but may not be freed
4. **Unused Function** (line 60): Helper functions may not be called

These issues are reported by the linter to help developers identify and fix problems before runtime.

## Learning Points

This example teaches:

- How to structure NexusLang programs
- How to use the development workflow
- How static analysis helps catch bugs early
- How type checking improves code quality
- How to enable runtime safety features
- How to interpret linter output and fix issues

## Next Steps

1. Fix the lint issues identified by running `nlpl lint`
2. Enable type checking with strict mode
3. Add more complex data processing
4. Experiment with different sanitizer configurations
5. Profile the program with `nlpl profile`
6. Add tests with `nlpl test`
