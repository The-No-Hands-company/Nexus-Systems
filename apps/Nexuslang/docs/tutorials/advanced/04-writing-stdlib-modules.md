# Tutorial 14: Writing Standard Library Modules

**Time:** ~60 minutes  
**Prerequisites:** [Building Projects](../intermediate/05-building-projects.md)

---

## Part 1 — Overview

The NexusLang standard library is implemented in Python.  Each module is a
subdirectory of `src/nexuslang/stdlib/` containing an `__init__.py` that
registers Python callables with the runtime.

When you import a module in NexusLang, the interpreter loads the corresponding
Python module and calls its `register_*_functions` function.

---

## Part 2 — Module File Layout

```
src/nexuslang/stdlib/
  my_module/
    __init__.py        # registration entry point
    _impl.py           # optional: implementation helpers
```

---

## Part 3 — Minimal Module

`src/nexuslang/stdlib/my_module/__init__.py`:

```python
"""
my_module: a demonstration stdlib module for NexusLang.
"""

from ...runtime.runtime import Runtime


def register_my_module_functions(runtime: Runtime) -> None:
    """Register all my_module functions with the runtime."""
    runtime.register_function("my_add",     my_add)
    runtime.register_function("my_greet",   my_greet)


def my_add(a, b):
    """Add two numbers."""
    if not (isinstance(a, (int, float)) and isinstance(b, (int, float))):
        raise TypeError(f"my_add expects two numbers, got {type(a)} and {type(b)}")
    return a + b


def my_greet(name: str) -> str:
    """Return a greeting string."""
    if not isinstance(name, str):
        raise TypeError(f"my_greet expects a string, got {type(name)}")
    return f"Hello, {name}!"
```

---

## Part 4 — Registering the Module

Open `src/nexuslang/stdlib/__init__.py` and add two lines:

```python
# 1. Import the registration function
from ..stdlib.my_module import register_my_module_functions

# 2. Add it to register_stdlib()
def register_stdlib(runtime: Runtime) -> None:
    # ... existing registrations ...
    register_my_module_functions(runtime)
```

---

## Part 5 — Using the Module from NexusLang

```nexuslang
import my_module

set result to my_module.my_add with 3 and 4
print text convert result to string    # 7

print text (my_module.my_greet with "Alice")    # Hello, Alice!
```

---

## Part 6 — Returning Multiple Values

Return a list (NLPL will destructure it):

```python
def divmod_nxl(a, b):
    if b == 0:
        raise ZeroDivisionError("division by zero")
    quotient, remainder = divmod(a, b)
    return [quotient, remainder]
```

```nexuslang
set parts to my_module.divmod_nxl with 17 and 5
set q to parts[0]    # 3
set r to parts[1]    # 2
```

---

## Part 7 — Accepting Callbacks

NLPL functions can be passed as arguments (they are Python callables at runtime):

```python
def apply_to_list(func, items):
    """Apply func to each element and return a new list."""
    if not callable(func):
        raise TypeError("apply_to_list: first argument must be a function")
    result = []
    for item in items:
        result.append(func(item))
    return result
```

```nexuslang
import my_module

function double with n as Integer returns Integer
    return n times 2
end

set nums to [1, 2, 3, 4, 5]
set doubled to my_module.apply_to_list with double and nums
# doubled = [2, 4, 6, 8, 10]
```

---

## Part 8 — Registering Constants

```python
def register_my_module_functions(runtime: Runtime) -> None:
    runtime.register_constant("MY_MAX_SIZE", 4096)
    runtime.register_constant("MY_VERSION",  "1.0")
    runtime.register_function("my_max_size", lambda: 4096)   # also as no-arg function
    # ... function registrations ...
```

```nexuslang
import my_module

if needed_size is greater than my_module.MY_MAX_SIZE
    raise error with "Request too large"
```

---

## Part 9 — Error Handling in Python Implementations

Raise Python built-in exceptions or NexusLang error types — the runtime
translates them into NexusLang runtime errors:

```python
from ....errors import NxlRuntimeError, NxlTypeError

def read_fixed_width_field(data: str, start: int, width: int) -> str:
    if not isinstance(data, str):
        raise NxlTypeError(f"Expected string, got {type(data).__name__}")
    if start < 0 or start + width > len(data):
        raise NxlRuntimeError(
            f"Field out of bounds: start={start}, width={width}, len={len(data)}"
        )
    return data[start : start + width]
```

---

## Part 10 — Writing Tests for Your Module

`tests/unit/stdlib/test_my_module.py`:

```python
import pytest
from nexuslang.runtime.runtime import Runtime
from nexuslang.stdlib.my_module import register_my_module_functions


@pytest.fixture
def runtime():
    rt = Runtime()
    register_my_module_functions(rt)
    return rt


def test_my_add(runtime):
    result = runtime.invoke_function("my_add", 3, 4)
    assert result == 7


def test_my_add_float(runtime):
    result = runtime.invoke_function("my_add", 1.5, 2.5)
    assert result == pytest.approx(4.0)


def test_my_add_type_error(runtime):
    with pytest.raises(TypeError):
        runtime.invoke_function("my_add", "a", 2)


def test_my_greet(runtime):
    result = runtime.invoke_function("my_greet", "World")
    assert result == "Hello, World!"
```

Run:

```bash
pytest tests/unit/stdlib/test_my_module.py -v
```

---

## Part 11 — Documenting Your Module

Add a reference page to `docs/reference/stdlib/my-module.md`:

```markdown
# my_module

Provides …

## Functions

### `my_add(a, b) -> Integer | Float`
Returns the sum of `a` and `b`.

### `my_greet(name) -> String`
Returns `"Hello, <name>!"`.

## Constants

### `MY_MAX_SIZE`
Maximum buffer size: `4096`.
```

---

## Summary

| Step | Action |
|------|--------|
| 1 | Create `src/nexuslang/stdlib/my_module/__init__.py` |
| 2 | Implement Python functions with type checks |
| 3 | Write `register_my_module_functions(runtime)` |
| 4 | Add registration call to `src/nexuslang/stdlib/__init__.py` |
| 5 | Write pytest tests in `tests/unit/stdlib/` |
| 6 | Add reference page to `docs/reference/stdlib/` |

**You have completed the Advanced Track!**

See also: [Contributing Guide](../contributing/) for code style and pull request requirements.
