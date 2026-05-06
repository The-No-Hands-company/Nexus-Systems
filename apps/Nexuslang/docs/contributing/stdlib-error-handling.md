# Standard Library Error Handling Convention

## Overview

All NexusLang standard library modules must use the unified error types defined in
`src/nexuslang/stdlib/errors.py` rather than raw Python exceptions. This ensures
NLPL programs receive consistent, catchable errors with rich context.

## Error Hierarchy

```
NLPLStdlibError                 -- base class for all stdlib errors
    NLPLValueError              -- invalid argument value
    NLPLIOError                 -- file/stream I/O failure
    NLPLKeyError                -- missing dictionary/map key
    NLPLIndexError              -- out-of-bounds index
    NLPLConnectionError         -- network connection failure
    NLPLTimeoutError            -- operation exceeded time limit
    NLPLPermissionError         -- insufficient permissions
    NLPLNotImplementedError     -- feature not yet available
    NLPLOverflowError           -- numeric overflow
    NLPLFormatError             -- string/data formatting error
    NLPLImportError             -- module import failure
```

## Usage Pattern

```python
from nexuslang.stdlib.errors import NLPLValueError, NLPLIOError

def stdlib_sqrt(x):
    if not isinstance(x, (int, float)):
        raise NLPLValueError(
            "sqrt requires a numeric argument",
            value=x,
            module="math",
            function="sqrt",
        )
    if x < 0:
        raise NLPLValueError(
            "sqrt requires a non-negative argument",
            value=x,
            module="math",
            function="sqrt",
        )
    return x ** 0.5
```

## Rules

1. **Never raise raw Python exceptions** (`ValueError`, `RuntimeError`, etc.)
   from stdlib code that NexusLang programs will call.

2. **Always include `module=` and `function=`** keyword arguments so error
   messages identify the source clearly.

3. **Use the most specific error type** available. For example, use
   `NLPLValueError` for bad argument values, not `NLPLStdlibError`.

4. **Wrap external exceptions** from Python libraries in the appropriate NexusLang
   error type:

   ```python
   try:
       result = external_library.do_something()
   except OSError as exc:
       raise NLPLIOError(
           f"I/O operation failed: {exc}",
           module="file_io",
           function="read_file",
       ) from exc
   ```

5. **Error messages should be human-readable** and describe what went wrong,
   not internal implementation details.

## Mapping from Python to NexusLang Error Types

| Python Exception     | NexusLang Error Type          |
|---------------------|--------------------------|
| `ValueError`        | `NLPLValueError`         |
| `RuntimeError`      | `NLPLStdlibError`        |
| `IOError` / `OSError` | `NLPLIOError`          |
| `KeyError`          | `NLPLKeyError`           |
| `IndexError`        | `NLPLIndexError`         |
| `ConnectionError`   | `NLPLConnectionError`    |
| `TimeoutError`      | `NLPLTimeoutError`       |
| `PermissionError`   | `NLPLPermissionError`    |
| `NotImplementedError` | `NLPLNotImplementedError` |
| `OverflowError`     | `NLPLOverflowError`      |
| `ImportError`       | `NLPLImportError`        |
| `TypeError`         | `NLPLValueError`         |

## Migration Status

Core modules migrated to unified error types:

- [x] `statistics`
- [x] `collections`
- [x] `modules`
- [x] `math` (uses stdlib_errors where applicable)

Remaining modules use legacy Python exceptions. They will be migrated
incrementally. New stdlib modules must use the unified types from the start.
