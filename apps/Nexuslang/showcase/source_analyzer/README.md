# Source Code Analyzer -- NexusLang Showcase Project

A practical CLI tool written entirely in NexusLang (~590 lines) that scans a directory of source files and produces a detailed analysis report with formatted tables and visual bars.

## Running

```bash
# From the NexusLang repo root:
PYTHONPATH=src python -m nexuslang.main showcase/source_analyzer/analyze.nlpl --no-type-check
```

By default, the analyzer scans `src/nlpl/parser`. To change the target, edit the `target_directory` variable at the top of `analyze.nlpl`.

## Report Sections

- **Summary** -- total files, lines, size, and a code/comment/blank breakdown with visual bars
- **Language Breakdown** -- per-language table (files, lines, code, comments, blank, size)
- **Language Distribution** -- visual bar chart comparing languages by line count
- **Largest Files** -- top 10 files ranked by size
- **Comment Density by Language** -- visual bars showing comment ratio per language

## Sample Output

```
================================================================================
  NexusLang Source Code Analyzer
  Scanning: src/nlpl/parser
================================================================================

  Found 9 files. Analyzing...

--------------------------------------------------------------------------------
  SUMMARY
--------------------------------------------------------------------------------

  Directory:     src/nlpl/parser
  Files found:   9
  Files analyzed: 9
  Total size:    571.0 KB

  Total lines:   13637
  Code lines:    9598
  Comment lines: 1693
  Blank lines:   2346

  Code:     [#####################.........] 70%
  Comments: [###...........................] 12%
  Blank:    [#####.........................] 17%

--------------------------------------------------------------------------------
  LANGUAGE BREAKDOWN
--------------------------------------------------------------------------------

  Language           Files     Lines      Code  Comments   Blank        Size
  --------------------------------------------------------------------------------
  Python                 7     13470      9455      1693    2322    564.8 KB
  Text                   1        78        64         0      14      3.8 KB
  Other                  1        89        79         0      10      2.3 KB
  --------------------------------------------------------------------------------
  TOTAL                  9     13637      9598      1693    2346    571.0 KB
```

## NexusLang Features Exercised

This program demonstrates a wide range of NexusLang language capabilities:

| Feature | Usage |
|---------|-------|
| **File I/O** | `walk_directory`, `read_file`, `get_file_size`, `get_extension`, `join_path` |
| **String Processing** | `strip`, `str_split`, `contains`, `substring`, `length`, `repeat` |
| **Type Conversion** | `convert X to integer`, `convert X to string` |
| **Control Flow** | `if`/`else`/`end`, `for each`/`end`, `while`/`end` |
| **Functions** | 7 user-defined functions with typed parameters and return types |
| **Collections** | Dict literals, dict access/mutation, list literals, `append`, `keys`, `dict_get` |
| **Error Handling** | `try`/`catch`/`end` with `return` inside try blocks |
| **Arithmetic** | Integer math, `integer divided by`, modulo, comparison operators |
| **Formatted Output** | Aligned tables, visual bar charts, padded columns |

## Development History

Building this tool initially revealed 5 usability gaps in NexusLang. All 5 were fixed at the
language implementation level (not worked around):

| Issue Found | Fix Applied |
|-------------|-------------|
| `label`, `split`, `starts_with`, `ends_with` were reserved keywords that collided with common identifiers | Removed unused token reservations; added `LABEL`/`REPEAT` to context-sensitive identifier set |
| `return` inside `try` blocks was caught as an exception | Made `execute_try_catch` re-raise `ReturnException` before the broad `except Exception` handler |
| No integer division in natural language syntax | Added `integer divided by` keyword mapping to `FLOOR_DIVIDE` token |
| `repeat()` stdlib function was shadowed by `REPEAT` keyword | Made `REPEAT` context-sensitive so it works as both keyword and identifier |
| No way to iterate dict keys | Added `keys()`, `values()`, `items()` stdlib functions |
