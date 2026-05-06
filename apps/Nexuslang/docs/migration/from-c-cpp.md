# Migrating from C / C++

This guide maps C and C++ patterns to their NexusLang equivalents.

---

## Syntax Quick-Reference

| Concept | C / C++ | NexusLang |
|---------|---------|------|
| Variable | `int x = 5;` | `set x as Integer to 5` |
| Print | `printf("%d\n", x);` | `print text convert x to string` |
| String concat | `strcat(buf, s)` | `a plus b` |
| Function | `int f(int a) { }` | `function f with a as Integer returns Integer ... end` |
| If | `if (c) { }` | `if c ... end` |
| For loop | `for (int i=0; i<n; i++)` | `for each i in range(0, n)` or `while` |
| While | `while (c) { }` | `while c` |
| Struct | `struct Point { int x; };` | `struct Point ... x as Integer ... end` |
| Class (C++) | `class Point { public: };` | `class Point ... end` |
| Constructor (C++) | `Point(int x) : x(x) {}` | `public function initialize with x as Integer` |
| Template (C++) | `template<typename T> T f(T v)` | `function f<T> that takes v as T returns T` |
| Try/catch (C++) | `try { } catch (exception&) { }` | `try ... catch error with message ... end` |
| Throw (C++) | `throw runtime_error("msg");` | `raise error with "msg"` |

---

## Pointers

NLPL supports the same pointer concepts as C, with a natural-language syntax.

### C pointer basics → NexusLang

```c
// C
int x = 42;
int *p = &x;
printf("%d\n", *p);
*p = 100;
```

```nlpl
# NexusLang
set x to 42
set p to address of x
print text convert (value at p) to string
set (value at p) to 100
```

### Pointer arithmetic

```c
// C
int arr[4] = {1, 2, 3, 4};
int *p = arr;
printf("%d\n", *(p + 2));   // 3
```

```nlpl
# NexusLang
set arr to [1, 2, 3, 4]
set p to address of arr[0]
set step to sizeof Integer
set third_ptr to (convert p to integer) plus (step times 2)
print text convert (value at (convert third_ptr to pointer)) to string
```

### Typed pointers

```c
// C
int *iptr = (int*)raw_ptr;
```

```nlpl
# NexusLang
set iptr to convert raw_ptr to Pointer to Integer
```

---

## Manual Memory Management

### C malloc / free → NexusLang allocate / free

```c
// C
uint8_t *buf = (uint8_t*)malloc(1024);
if (!buf) { fprintf(stderr, "OOM\n"); exit(1); }
// ... use buf ...
free(buf);
```

```nlpl
# NexusLang
set buf to allocate buf of size 1024 bytes
try
    # ... use buf ...
always
    free buf
end
```

The `always` block guarantees cleanup even if an error occurs — similar to a destructor or RAII wrapper.

### sizeof

```c
size_t n = sizeof(int);
```

```nlpl
set n to sizeof Integer
```

---

## Structs

```c
// C
typedef struct {
    float x;
    float y;
} Point;

Point p = {3.0f, 4.0f};
printf("%.1f\n", p.x);
```

```nlpl
# NexusLang
struct Point
    x as Float
    y as Float
end

set p to new Point
set p.x to 3.0
set p.y to 4.0
print text convert p.x to string
```

---

## Classes (C++ Style)

```cpp
// C++
class Animal {
public:
    std::string name;
    Animal(std::string n) : name(n) {}
    virtual std::string speak() { return name + " makes a sound"; }
};

class Dog : public Animal {
public:
    Dog(std::string n) : Animal(n) {}
    std::string speak() override { return name + " barks"; }
};
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

## Templates / Generics

```cpp
// C++
template<typename T>
T max_value(T a, T b) {
    return (a > b) ? a : b;
}
```

```nlpl
# NexusLang
function max_value<T> that takes a as T and b as T returns T
    if a is greater than b
        return a
    return b
end
```

---

## Inline Assembly

```c
// GCC inline ASM
int result;
__asm__ volatile (
    "mov $1, %%eax\n\t"
    "mov $2, %%ebx\n\t"
    "add %%ebx, %%eax"
    : "=a"(result)
    :
    : "ebx"
);
```

```nlpl
# NexusLang
set result as Integer to 0
asm volatile
    code "mov $1, %0"
    code "add $2, %0"
    outputs "=r": result
    clobbers "cc"
end
print text convert result to string    # 3
```

Key differences:

| GCC | NexusLang |
|-----|------|
| `__asm__ volatile(...)` | `asm volatile ... end` |
| Multi-line strings | Separate `code` lines |
| `"=r"(var)` | `outputs "=r": var` |
| `"r"(var)` | `inputs "r": var` |
| `"memory", "eax"` | `clobbers "memory", "eax"` |

---

## Calling C Libraries (FFI)

### In C you link directly; in NexusLang use `extern function`

```c
// C — just include a header
#include <string.h>
size_t n = strlen("hello");
```

```nlpl
# NexusLang — declare the extern then call it
extern function strlen with s as Pointer returns Integer from library "c"

set n to call strlen with "hello"
print text convert n to string    # 5
```

More complex example — zlib:

```c
// C
#include <zlib.h>
uLong crc = crc32(0L, Z_NULL, 0);
crc = crc32(crc, (Bytef*)data, len);
```

```nlpl
# NexusLang
extern function crc32_init with seed as Integer and buf as Pointer and len as Integer returns Integer from library "z"

set crc to call crc32_init with 0 and null and 0
```

---

## Error Handling

C uses return codes; C++ uses exceptions. NexusLang uses `try/catch`.

```c
// C — return code pattern
FILE *f = fopen("data.txt", "r");
if (!f) {
    perror("fopen");
    return -1;
}
```

```nlpl
# NexusLang
import io
try
    set content to io.read_file with "data.txt"
catch error with message
    print text "Could not open file: " plus message
    return -1
end
```

```cpp
// C++ — exception pattern
try {
    auto data = risky_op();
} catch (const std::runtime_error& e) {
    std::cerr << e.what() << "\n";
}
```

```nlpl
# NexusLang
try
    set data to risky_op()
catch error with message
    print text message
end
```

---

## Header Files

C/C++ uses `.h`/`.hpp` headers for declarations. NexusLang modules have no separate declaration files; import the module directly.

```c
// C — mylib.h + mylib.c
void greet(const char *name);   // declaration in .h
```

```nlpl
# NexusLang — mylib.nlpl (single file, no header needed)
function greet with name as String
    print text "Hello, " plus name
end
export greet
```

```nlpl
# Importing
from mylib import greet
greet with "Alice"
```

---

## Build System

| C / C++ | NexusLang |
|---------|------|
| `Makefile` / CMake | `nexuslang.toml` |
| `gcc -o out main.c` | `nexuslang build` |
| `gcc -O2 ...` | `[profile.release] optimization = 2` in `nexuslang.toml` |
| `make test` | `nexuslang test` |
| `pkg-config` | `[dependencies]` in `nexuslang.toml` |
| `#include <lib.h>` | `import lib` (stdlib) or `extern function ...` (C lib) |

Example `nexuslang.toml`:

```toml
[package]
name = "myapp"
version = "1.0.0"

[[bin]]
name = "myapp"
source = "src/main.nxl"

[profile.release]
optimization = 3
link_time_optimization = true
```

---

## Key Differences Summary

| Topic | C / C++ | NexusLang |
|-------|---------|------|
| Memory | Manual malloc/free | Automatic + optional manual |
| Strings | `char*` / `std::string` | `String` (built-in, UTF-8) |
| Null safety | Unchecked null pointers | Null checks; optional pointer validation |
| Error handling | Return codes / exceptions | `try/catch/always` |
| Headers | `.h` / `.hpp` required | No headers; single-file modules |
| Undefined behavior | Possible (buffer overflows, etc.) | Memory safety tools available (`MemorySafetyValidator`) |
| Templates | `template<typename T>` | `function f<T> that takes v as T returns T` |
| Inline ASM | `__asm__` / `asm` blocks | `asm ... end` blocks |
| Build | Make / CMake / Meson | `nexuslang build` + `nexuslang.toml` |
