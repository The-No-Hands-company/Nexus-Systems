# Migrating from Python

This guide maps Python patterns to their NexusLang equivalents.

---

## Syntax Quick-Reference

| Concept | Python | NexusLang |
|---------|--------|------|
| Assign variable | `x = 5` | `set x to 5` |
| Typed variable | `x: int = 5` | `set x as Integer to 5` |
| Print | `print(x)` | `print text x` |
| f-string | `f"hello {name}"` | `"hello " plus name` |
| Comment | `# comment` | `# comment` |
| Function | `def f(a, b):` | `function f with a as Type and b as Type returns Type` |
| Default param | `def f(x=0):` | `function f with x as Integer default to 0` |
| Call function | `f(1, 2)` | `f with 1 and 2` |
| Named call | `f(a=1, b=2)` | `f with a: 1 and b: 2` |
| Return | `return x` | `return x` |
| If / elif / else | `if c: ... elif c: ...` | `if c ... else if c ... else ...` |
| For loop | `for item in items:` | `for each item in items` |
| While loop | `while x < 5:` | `while x is less than 5` |
| List literal | `[1, 2, 3]` | `[1, 2, 3]` |
| Dict literal | `{"a": 1}` | `{"a": 1}` |
| Class | `class Foo:` | `class Foo` |
| Inherit | `class Bar(Foo):` | `class Bar extends Foo` |
| Constructor | `def __init__(self, x):` | `public function initialize with x as Type` |
| Self | `self.x = val` | `set this.x to val` |
| Try / except | `try: ... except E as e:` | `try ... catch error with message ... end` |
| Raise | `raise ValueError("msg")` | `raise error with "msg"` |
| Import module | `import os` | `import io` |
| Import name | `from os.path import join` | `from io import join_path` |
| Async function | `async def f():` | `async function f` |
| Await | `await coro()` | `await coro()` |
| Concurrent | `asyncio.gather(a(), b())` | `await gather with [a(), b()]` |

---

## Variables and Types

Python uses dynamic typing; NexusLang supports optional static types.

### Dynamic style (works fine)

```nlpl
set name to "Alice"
set count to 42
set ratio to 3.14
```

### Explicit types (recommended for larger programs)

```nlpl
set name  as String  to "Alice"
set count as Integer to 42
set ratio as Float   to 3.14
```

NLPL core types:

| Python | NexusLang |
|--------|------|
| `int` | `Integer` |
| `float` | `Float` |
| `str` | `String` |
| `bool` | `Boolean` |
| `list` | `List` |
| `dict` | `Dictionary` |
| `None` | `null` |

---

## Functions

```python
# Python
def greet(name: str, greeting: str = "Hello") -> str:
    return f"{greeting}, {name}!"
```

```nlpl
# NexusLang
function greet with name as String and greeting as String default to "Hello" returns String
    return greeting plus ", " plus name plus "!"
end
```

Calling with named arguments:

```nlpl
set msg to greet with name: "Bob" and greeting: "Hi"
```

Variadic arguments:

```python
# Python
def log(*messages):
    for m in messages:
        print(m)
```

```nlpl
# NexusLang
function log with *messages as String
    for each m in messages
        print text m
end
```

---

## Classes

```python
# Python
class Animal:
    def __init__(self, name: str):
        self.name = name

    def speak(self) -> str:
        return f"{self.name} makes a sound"

class Dog(Animal):
    def speak(self) -> str:
        return f"{self.name} barks"
```

```nlpl
# NexusLang
class Animal
    public set name to String

    public function initialize with name as String
        set this.name to name

    public function speak returns String
        return this.name plus " makes a sound"
end

class Dog extends Animal
    public function speak returns String
        return this.name plus " barks"
end

set d to create Dog with "Rex"
print text d.speak()
```

---

## Error Handling

```python
# Python
try:
    result = risky_op()
except ValueError as e:
    print(f"Error: {e}")
finally:
    cleanup()
```

```nlpl
# NexusLang
try
    set result to risky_op()
catch error with message
    print text "Error: " plus message
always
    cleanup()
end
```

Raise an error:

```python
raise ValueError("invalid input")
```

```nlpl
raise error with "invalid input"
```

---

## Standard Library Equivalents

| Python module | NexusLang module | Notes |
|---------------|-------------|-------|
| `os`, `os.path` | `io` | File I/O and path helpers |
| `pathlib` | `io` | `io.join_path`, `io.exists`, `io.is_directory` |
| `json` | `io` | `io.parse_json`, `io.to_json` |
| `csv` | `io` | `io.read_csv`, `io.write_csv` |
| `math` | `math` | Same function names; `math.sqrt`, `math.sin`, etc. |
| `random` | `math` | `math.random`, `math.random_int` |
| `re` | `regex` | `regex.match`, `regex.find_all`, `regex.replace` |
| `requests`, `httpx` | `network` | `network.http_get`, `network.http_post` |
| `socket` | `network` | `network.create_server`, `network.connect` |
| `threading`, `asyncio` | `system` | Threads; `async`/`await` built into the language |
| `hashlib` | `crypto` | `crypto.sha256`, `crypto.md5` |
| `time` | `system` | `system.current_time_ms`, `system.sleep` |

---

## List and Dictionary Operations

```python
# Python
nums = [1, 2, 3, 4, 5]
doubled = [x * 2 for x in nums]
evens = [x for x in nums if x % 2 == 0]
```

```nlpl
# NexusLang  — manual loops (comprehensions not yet available)
set nums to [1, 2, 3, 4, 5]

set doubled to []
for each x in nums
    append (x times 2) to doubled

set evens to []
for each x in nums
    if (x modulo 2) equals 0
        append x to evens
```

---

## Modules and Imports

```python
# Python (my_module.py)
def greet(name):
    return f"Hello, {name}"
```

```nlpl
# NexusLang (my_module.nxl)
function greet with name as String returns String
    return "Hello, " plus name
end
export greet
```

```nlpl
# Importing
import my_module
set msg to my_module.greet with "Alice"
```

---

## Performance Notes for Python Developers

1. **Type annotations speed things up.** Unlike Python, NexusLang can use type hints for optimised code paths.
2. **Use `async/await` for I/O-bound work.** Same pattern as Python `asyncio`.
3. **Use threads for CPU-bound work.** `system.spawn_thread` / `system.join_thread`.
4. **Avoid building large strings in a loop.** Collect parts in a list, then `io.join`.
5. **Profiles first, optimise second.** Run with `--profile` to find hot spots before rewriting.
