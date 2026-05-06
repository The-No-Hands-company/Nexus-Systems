# NexusLang Development Tooling Guide

## Overview

NLPL includes three powerful development tools to enhance code quality, performance, and maintainability:

1. **Static Analyzer** - Detect bugs, security issues, and code quality problems
2. **Code Formatter** - Enforce consistent style and readability  
3. **Runtime Profiler** - Identify performance bottlenecks

## Static Analyzer

The static analyzer performs multi-pass analysis to detect potential issues before runtime.

### Usage

```bash
# Analyze single file
./nexuslang-analyze examples/01_basic_concepts.nlpl

# Analyze multiple files
./nexuslang-analyze test_programs/unit/basic/*.nlpl

# Export to JSON
./nexuslang-analyze --json report.json examples/*.nlpl
```

### What It Detects

**Errors** (Critical issues that will likely cause runtime failures):
- Undefined variables
- Calls to undefined functions
- Return statements outside functions
- Parse errors

**Warnings** (Suspicious code that might indicate bugs):
- Unused variables
- Variable shadowing
- Constant conditions in if/while statements
- Infinite loops (while true)
- Missing return statements in typed functions
- Comparisons between literals

**Info** (Style and optimization suggestions):
- Empty classes
- Variables initialized to null

### Example Output

```
Found 25 issues:
  Errors: 8
  Warnings: 17
  Info: 0

ERROR:     file.nlpl:5  [undefined-function] Call to undefined function 'length'
WARNING:   file.nlpl:12 [unused-variable] Variable 'result' is defined but never used
           Suggestion: Remove unused variable or prefix with '_' if intentional
INFO:      file.nlpl:20 [empty-class] Class 'MyClass' has no properties or methods
           Suggestion: Consider if this class is necessary
```

### Integration

```python
from nexuslang.tools import StaticAnalyzer, Severity

analyzer = StaticAnalyzer(strict=False)
issues = analyzer.analyze_file("my_program.nxl")

# Filter by severity
errors = [i for i in issues if i.severity == Severity.ERROR]
for error in errors:
    print(f"{error.file}:{error.line} - {error.message}")
```

## Code Formatter

The formatter enforces consistent code style while preserving program semantics.

### Usage

```bash
# Preview formatting (prints to stdout)
./nexuslang-format my_program.nlpl

# Format in-place
./nexuslang-format -i my_program.nlpl

# Show diff of changes
./nexuslang-format --diff my_program.nlpl

# Check if file needs formatting (exit code 1 if changes needed)
./nexuslang-format --check my_program.nlpl
```

### Formatting Rules

- **Indentation**: 4 spaces (configurable)
- **Operators**: Space before and after (`a + b`, not `a+b`)
- **Commas**: Space after, none before (`[1, 2, 3]`)
- **Line length**: 100 characters maximum (configurable)
- **Blank lines**: 2 after functions and classes, 1 after imports
- **Trailing whitespace**: Removed
- **Excessive blank lines**: Maximum 2 consecutive

### Example

**Before:**
```nexuslang
set x to    5
set   y   to   10



function badly_formatted with a as Integer and b as Integer returns Integer
set result to a+b*2
return result
end



set   list   to  [  1 ,  2,3  ,   4]
```

**After:**
```nexuslang
set x to 5
set y to 10

function badly_formatted with a as Integer and b as Integer returns Integer
    set result to a + b * 2
    return result


set list to [1, 2, 3, 4]
```

### Configuration

```python
from nexuslang.tools import Formatter, FormatConfig

config = FormatConfig(
    indent_size=4,
    max_line_length=100,
    blank_lines_after_function=2,
    blank_lines_after_class=2,
    blank_lines_after_import=1
)

formatter = Formatter(config)
formatted = formatter.format_file("my_program.nxl")
```

## Runtime Profiler

The profiler tracks execution performance to identify hotspots and optimization opportunities.

### Usage

**Option 1: Integrate with interpreter**

```bash
# Run with profiling enabled (add to main.py --profile flag)
python src/main.py --profile my_program.nlpl
```

**Option 2: Programmatic integration**

```python
from nexuslang.tools import Profiler, enable_profiling

# Enable profiling
profiler = enable_profiling()

# ... run your program ...

# Stop and print report
profiler.stop()
profiler.print_report()
```

**Option 3: Manual instrumentation**

```python
from nexuslang.tools import get_profiler

profiler = get_profiler()
profiler.start()

# Track function calls
profiler.enter_function("my_function")
# ... function body ...
profiler.exit_function("my_function")

# Track line execution
profiler.record_line("file.nxl", 42)

# Track memory allocation
profiler.record_allocation(1024)  # bytes
profiler.record_deallocation(1024)

# Generate report
profiler.stop()
profiler.print_report()
```

### Features

**Function Profiling**:
- Call count
- Total time and self time (excluding child calls)
- Min/max/average execution time
- Caller/callee relationships

**Line Profiling**:
- Execution count per line
- Identify hot lines

**Memory Tracking**:
- Total allocations and deallocations
- Current memory usage
- Peak memory usage

### Example Output

```
=== Profiling Report ===
Total time: 5.234s
Functions profiled: 12
Memory: 2.5 MB current, 8.3 MB peak

Top 10 Hottest Functions:
  fibonacci        1000 calls    4.123s total    0.004s avg
  calculate_sum     500 calls    0.892s total    0.002s avg
  process_data      100 calls    0.156s total    0.002s avg

Top 10 Hottest Lines:
  main.nlpl:42     15000 hits
  utils.nlpl:18     8500 hits
  core.nlpl:92      5200 hits

Memory:
  Allocations: 12,534 (125.2 MB)
  Deallocations: 11,890 (118.7 MB)
  Current: 2.5 MB
  Peak: 8.3 MB
```

### Export Formats

**Text Report**:
```python
profiler.print_report()  # Print to stdout
```

**JSON Export**:
```python
profiler.export_json("profile_results.json")
```

JSON format:
```json
{
  "total_time": 5.234,
  "memory": {
    "allocations": 12534,
    "deallocations": 11890,
    "current_bytes": 2621440,
    "peak_bytes": 8703180
  },
  "functions": {
    "fibonacci": {
      "call_count": 1000,
      "total_time": 4.123,
      "self_time": 3.456,
      "min_time": 0.001,
      "max_time": 0.012,
      "avg_time": 0.004
    }
  },
  "lines": {
    "main.nlpl:42": 15000,
    "utils.nlpl:18": 8500
  }
}
```

**Flamegraph Export**:
```python
profiler.export_flamegraph("profile.folded")
```

Then visualize with flamegraph tools:
```bash
# Install flamegraph (https://github.com/brendangregg/FlameGraph)
git clone https://github.com/brendangregg/FlameGraph
./FlameGraph/flamegraph.pl profile.folded > profile.svg
# Open profile.svg in browser
```

## Integration with NexusLang Interpreter

To add profiler support to the interpreter, update `src/main.py`:

```python
import argparse
from nexuslang.tools import enable_profiling, disable_profiling, get_profiler

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('file', help='NLPL source file')
    parser.add_argument('--profile', action='store_true', help='Enable profiling')
    parser.add_argument('--profile-output', help='Save profile results to file')
    args = parser.parse_args()
    
    # Enable profiling if requested
    if args.profile:
        profiler = enable_profiling()
    
    # ... run interpreter ...
    
    # Print/save profile results
    if args.profile:
        profiler.stop()
        if args.profile_output:
            profiler.export_json(args.profile_output)
        else:
            profiler.print_report()
```

Integrate into the interpreter loop:

```python
# In interpreter.py execute() method
def execute(self, node):
    if self.profiler:
        self.profiler.record_line(self.current_file, node.line_number)
    
    # ... execute node ...

def visit_function_call(self, node):
    if self.profiler:
        self.profiler.enter_function(node.name)
    
    result = self._execute_function(node)
    
    if self.profiler:
        self.profiler.exit_function(node.name)
    
    return result
```

## CI/CD Integration

### GitHub Actions Example

```yaml
name: Code Quality
on: [push, pull_request]

jobs:
  analyze:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - name: Setup Python
        uses: actions/setup-python@v4
        with:
          python-version: '3.11'
      - name: Install dependencies
        run: pip install -r requirements.txt
      - name: Run static analyzer
        run: |
          ./nexuslang-analyze examples/*.nlpl --json analysis.json
          # Fail if critical errors found
          python -c "
          import json
          with open('analysis.json') as f:
              issues = json.load(f)
          errors = [i for i in issues if i['severity'] == 'ERROR']
          if errors:
              print(f'Found {len(errors)} critical errors')
              exit(1)
          "
      - name: Check formatting
        run: |
          ./nexuslang-format --check examples/*.nlpl
```

### Pre-commit Hook

Create `.git/hooks/pre-commit`:

```bash
#!/bin/bash
# Run analyzer on staged .nlpl files
STAGED=$(git diff --cached --name-only --diff-filter=ACM | grep '\.nxl$')

if [ -n "$STAGED" ]; then
    echo "Running static analysis..."
    ./nexuslang-analyze $STAGED
    
    if [ $? -ne 0 ]; then
        echo "Static analysis found critical issues. Commit aborted."
        exit 1
    fi
    
    echo "Checking code formatting..."
    ./nexuslang-format --check $STAGED
    
    if [ $? -ne 0 ]; then
        echo "Code needs formatting. Run './nexuslang-format -i <files>' and try again."
        exit 1
    fi
fi
```

Make executable: `chmod +x .git/hooks/pre-commit`

## Best Practices

### Static Analysis

1. **Run regularly**: Integrate into CI/CD pipeline
2. **Fix errors first**: Address critical issues before warnings
3. **Use intentional prefixes**: Prefix unused variables with `_` if intentional
4. **Review warnings**: They often indicate real bugs

### Code Formatting

1. **Format before commit**: Keep codebase consistent
2. **Use `--check` in CI**: Enforce formatting in pull requests
3. **Configure per-project**: Adjust indent size and line length as needed
4. **Format entire codebase**: When introducing formatter, format all files at once

### Profiling

1. **Profile realistic workloads**: Use representative data and scenarios
2. **Focus on hotspots**: Optimize the top 10 functions/lines first
3. **Compare before/after**: Measure impact of optimizations
4. **Export results**: Track performance over time
5. **Use flamegraphs**: Visualize call stacks for complex programs

## Troubleshooting

### Analyzer Issues

**"Parse error" on valid code**:
- Check for syntax extensions not yet supported
- Report bug with minimal reproduction case

**Too many false positives**:
- Some stdlib functions may not be recognized as builtins
- Update `is_builtin()` in analyzer.py

**Missing line numbers** (shows 0 or None):
- Known limitation: some AST nodes lack line_number attributes
- Doesn't affect correctness of analysis

### Formatter Issues

**Formatter changes behavior**:
- File a bug report immediately - formatter should preserve semantics
- Include before/after code

**Line too long after formatting**:
- Manually break line or adjust max_line_length config
- Consider refactoring complex expressions

### Profiler Issues

**High memory overhead**:
- Line profiling can be expensive for hot loops
- Disable line profiling, use function profiling only

**Inaccurate timing**:
- Python's profiling overhead affects small functions
- Focus on relative differences, not absolute times
- For micro-benchmarks, use compiled mode instead

## Tool Comparison

| Feature | Analyzer | Formatter | Profiler |
|---------|----------|-----------|----------|
| Static analysis | ✓ | - | - |
| Runtime analysis | - | - | ✓ |
| Code modification | - | ✓ | - |
| CI/CD integration | ✓ | ✓ | ✓ |
| Zero overhead | ✓ | ✓ | - |
| Line-level detail | ✓ | ✓ | ✓ |

## Future Enhancements

Planned improvements:

- **Analyzer**: Data flow analysis, taint tracking, more security checks
- **Formatter**: Auto-fix common issues (unused imports, etc.)
- **Profiler**: Hardware counter integration, thread profiling, heap profiling

## Summary

The NexusLang tooling suite provides production-quality code analysis, formatting, and profiling capabilities:

- **Analyze** code for bugs and quality issues with `nexuslang-analyze`
- **Format** code consistently with `nexuslang-format`
- **Profile** runtime performance with the integrated profiler

All tools work seamlessly with the NexusLang interpreter and compiler, helping you write better, faster, and more maintainable code.
