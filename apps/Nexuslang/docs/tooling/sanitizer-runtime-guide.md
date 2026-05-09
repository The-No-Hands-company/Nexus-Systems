# Sanitizer Runtime Output Guide

This guide explains how to run NLPL programs with sanitizers and interpret the structured runtime output.

## Enable Sanitizers

Build with one or more sanitizers:

```bash
nexuslang build --sanitize address
nexuslang build --sanitize thread undefined
nexuslang build --sanitize all
```

Run with runtime analysis enabled:

```bash
nexuslang run --sanitize address --analyze-runtime
```

By default, runtime analysis is written to:

```text
build/runtime-analysis/
```

You can override this path:

```bash
nexuslang run --sanitize address --analyze-runtime --analysis-output build/analysis
```

## What Gets Generated

When sanitizer findings are detected, these artifacts are produced:

- `sanitizer-report.json`: Structured sanitizer findings.
- `combined-runtime-analysis.json`: Sanitizer findings + profiling summary.
- `profile/` directory (when runtime analysis profiling source is available).

## Sanitizer Report Format

`sanitizer-report.json` contains:

```json
{
  "finding_count": 1,
  "findings": [
    {
      "sanitizer": "AddressSanitizer",
      "error_type": "heap-use-after-free",
      "summary": "==42==ERROR: AddressSanitizer: heap-use-after-free ...",
      "location": "src/main.c:10:5",
      "raw_line": "==42==ERROR: AddressSanitizer: heap-use-after-free ..."
    }
  ]
}
```

## Interpreting Common Findings

### AddressSanitizer

Typical error types:

- `heap-use-after-free`: Memory accessed after deallocation.
- `heap-buffer-overflow`: Out-of-bounds heap access.
- `stack-buffer-overflow`: Out-of-bounds stack access.
- `double-free`: Memory released more than once.

How to fix:

1. Check the reported location and call chain.
2. Verify ownership/lifetime for pointers.
3. Add bounds checks before index/pointer operations.
4. Null out freed pointers and guard re-use.

### ThreadSanitizer

Typical error types:

- `data race`: Unsynchronized concurrent access.
- `lock-order-inversion`: Inconsistent lock ordering.

How to fix:

1. Protect shared state with mutexes/channels.
2. Use a global lock ordering strategy.
3. Avoid unsafely shared mutable state.

### MemorySanitizer

Typical error types:

- `use-of-uninitialized-value`: Read before initialization.

How to fix:

1. Initialize variables at declaration.
2. Ensure all conditional paths assign values.
3. Validate function return values before use.

### UndefinedBehaviorSanitizer

Typical error types:

- Integer overflow.
- Invalid shifts.
- Misaligned access.
- Null dereference in UB contexts.

How to fix:

1. Add explicit overflow checks.
2. Validate shift ranges and divisors.
3. Use checked arithmetic helpers where available.

## Combined Runtime Analysis

`combined-runtime-analysis.json` merges runtime sanitizer output with profiler summary data for a single run.

This lets you prioritize fixes by combining:

- Correctness risk (`sanitizer.findings`).
- Runtime impact (`profiling.cpu_functions`, `profiling.memory_peak_bytes`).

## Integrate Existing Profile Runs with Sanitizer Logs

If you already have a sanitizer stderr log and want to merge it with profile output:

```bash
nexuslang profile src/main.nxl --output profile --sanitizer-log sanitizer.log
```

This generates profile artifacts and writes merged runtime analysis in the same output directory.
