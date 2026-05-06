# Migrating from Rust

This guide maps Rust patterns to their NexusLang equivalents.

---

## Syntax Quick-Reference

| Concept | Rust | NexusLang |
|---------|------|------|
| Immutable binding | `let x = 5;` | `set x to 5` |
| Mutable binding | `let mut x = 5;` | `set x to 5` (all vars mutable by default) |
| Typed variable | `let x: i32 = 5;` | `set x as Integer to 5` |
| Print | `println!("{x}");` | `print text x` |
| String concat | `format!("{a}{b}")` | `a plus b` |
| Function | `fn f(a: T) -> R { }` | `function f with a as T returns R ... end` |
| Closure | `\|x\| x * 2` | `function handler with x ... end` |
| If | `if c { }` | `if c ... end` |
| For loop | `for item in items { }` | `for each item in items` |
| While | `while c { }` | `while c` |
| Struct | `struct Point { x: i32 }` | `struct Point ... x as Integer ... end` |
| Impl block | `impl Point { fn new() {} }` | `class Point ... function initialize ... end` |
| Match | `match x { 1 => ... }` | `match x with case 1 ... end` |
| Error propagation | `result?` | Re-raise inside `try/catch` |
| Trait | `trait Drawable { }` | `interface Drawable ... end` |
| Generic | `fn f<T>(v: T) -> T` | `function f<T> that takes v as T returns T` |
| Async fn | `async fn f() { }` | `async function f ... end` |
| Await | `f().await` | `await f()` |

---

## Memory Management

Rust enforces ownership and borrowing at compile time. NexusLang provides automatic memory management for most programs and explicit manual control via pointers when you need it.

### Rust ownership → NexusLang automatic management

```rust
// Rust — explicit move semantics
let s1 = String::from("hello");
let s2 = s1;  // s1 is moved, cannot be used
```

```nlpl
# NexusLang — values are reference-counted; both names are valid
set s1 to "hello"
set s2 to s1
print text s1    # fine
print text s2    # fine
```

### Manual memory (when you need it)

```rust
// Rust
let raw: *mut u8 = unsafe { libc::malloc(64) as *mut u8 };
unsafe { libc::free(raw as *mut libc::c_void) };
```

```nlpl
# NexusLang
set buf to allocate buf of size 64 bytes
# ... use buf ...
free buf
```

**Rule of thumb**: only reach for manual allocation in performance-critical paths or when interfacing with C.

---

## Structs and Classes

```rust
// Rust
struct Point {
    x: f32,
    y: f32,
}

impl Point {
    fn new(x: f32, y: f32) -> Self {
        Point { x, y }
    }
    fn distance(&self) -> f32 {
        (self.x * self.x + self.y * self.y).sqrt()
    }
}
```

```nlpl
# NexusLang
import math

class Point
    public set x to Float
    public set y to Float

    public function initialize with x as Float and y as Float
        set this.x to x
        set this.y to y

    public function distance returns Float
        return math.sqrt(this.x times this.x plus this.y times this.y)
end

set p to create Point with 3.0 and 4.0
print text convert p.distance() to string    # 5.0
```

---

## Traits vs Interfaces

```rust
// Rust
trait Shape {
    fn area(&self) -> f32;
}

impl Shape for Circle {
    fn area(&self) -> f32 { std::f32::consts::PI * self.radius * self.radius }
}
```

```nlpl
# NexusLang
interface Shape
    function area returns Float
end

class Circle implements Shape
    public set radius to Float

    public function initialize with radius as Float
        set this.radius to radius

    public function area returns Float
        return 3.14159 times this.radius times this.radius
end
```

---

## Error Handling

### Rust `Result<T, E>` → NexusLang `try/catch`

```rust
// Rust
fn read_name(path: &str) -> Result<String, std::io::Error> {
    let content = std::fs::read_to_string(path)?;
    Ok(content.trim().to_string())
}
```

```nlpl
# NexusLang
import io

function read_name with path as String returns String
    try
        set content to io.read_file with path
        return io.trim with content
    catch error with message
        raise error with "Could not read name: " plus message
end
```

### Rust `Option<T>` → NexusLang null checks

```rust
// Rust
let maybe: Option<i32> = Some(42);
if let Some(v) = maybe {
    println!("{v}");
}
```

```nlpl
# NexusLang
set maybe to 42    # or null
if maybe is not null
    print text convert maybe to string
```

---

## Generics

```rust
// Rust
fn largest<T: PartialOrd>(list: &[T]) -> &T {
    let mut largest = &list[0];
    for item in list { if item > largest { largest = item; } }
    largest
}
```

```nlpl
# NexusLang
function largest<T> that takes items as List of T returns T
    set best to items[0]
    for each item in items
        if item is greater than best
            set best to item
    return best
end
```

---

## Async / Await

The NexusLang model is similar to Rust's `async`/`.await`, but without the need to choose an executor.

```rust
// Rust (with Tokio)
use tokio;

#[tokio::main]
async fn main() {
    let (a, b) = tokio::join!(fetch_a(), fetch_b());
}
```

```nlpl
# NexusLang
async function main
    set results to await gather with [fetch_a(), fetch_b()]
    set a to results[0]
    set b to results[1]
end
```

---

## FFI

### Calling C from Rust

```rust
extern "C" {
    fn strlen(s: *const std::ffi::c_char) -> usize;
}
```

### Calling C from NexusLang

```nlpl
extern function strlen with s as Pointer returns Integer from library "c"
```

Both require unsafe/care — in NexusLang, validation helpers are available from `nlpl.security`:

```nlpl
from nexuslang.security import validate_pointer

if validate_pointer with strlen_result
    print text "Pointer is valid"
```

---

## Pattern Matching

```rust
// Rust
match status {
    200 => println!("OK"),
    404 => println!("Not Found"),
    _   => println!("Other"),
}
```

```nlpl
# NexusLang
match status with
    case 200
        print text "OK"
    case 404
        print text "Not Found"
    default
        print text "Other"
end
```

---

## Key Differences Summary

| Topic | Rust | NexusLang |
|-------|------|------|
| Memory safety | Borrow checker | Automatic + optional manual |
| Null safety | `Option<T>` | Null checks (`is null`) |
| Error handling | `Result<T,E>`, `?` | `try/catch/always` |
| Concurrency model | `async`/`threads`, `Send`/`Sync` | `async`/`await`, `system.spawn_thread` |
| Macros | `macro_rules!`, proc macros | No macros; use functions or stdlib |
| Build system | Cargo + `Cargo.toml` | `nexuslang build` + `nexuslang.toml` |
| Package registry | crates.io | NexusLang package registry (planned) |
