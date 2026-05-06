# Key Features

## Language features

### Natural syntax
```nlpl
set x to 10
if x is greater than 5
    print text "big"
end
```

### Full OOP
Classes, inheritance, interfaces, traits, abstract classes, mixins.

### Generics with HKT
```nlpl
class Container with T :: *
    set value as T

    function get returns T
        return value
    end
end
```

### Pattern matching
```nlpl
match shape
    case Circle with radius
        print text "Circle, r=" plus radius
    case Rectangle with width, height
        print text "Rect " plus width plus "x" plus height
    case _
        print text "Unknown"
end
```

### FFI (Foreign Function Interface)
Call any C library directly:
```nlpl
extern function printf with format as CString, value as Integer
set _ to printf("Value: %d\n", 42)
```

### Inline assembly
```nlpl
inline asm "nop" : : :
```

### Smart pointers
```nlpl
set ptr to Rc(create MyStruct)
set clone to ptr.clone()
```

### Error handling
```nlpl
try
    set result to parse_json(raw_input)
catch ParseError as e
    print text "Parse failed: " plus e.message
end
```

### Closures and lambdas
```nlpl
set double to function with x returns x times 2
set results to map(numbers, double)
```

### Async/await
```nlpl
async function fetch_data with url as String returns String
    set response to await http_get(url)
    return response.body
end
```

## Type system

- Full type inference — types are optional but enforced when declared
- Generic types with constraints
- Higher-kinded types (`T :: * -> *`)
- Borrow checker (Rust-inspired, opt-in)
- Lifetime annotations
- Union types
- Optional/Result types from stdlib

## Standard library (85 modules)

2,219 registered functions covering:

| Category | Modules |
|----------|---------|
| Core | math, string, collections, I/O, system |
| Data | JSON, CSV, XML, SQLite, databases |
| Networking | HTTP client/server, WebSocket, TCP/UDP |
| Concurrency | async runtime, threading, atomics, sync primitives |
| Systems | FFI, inline assembly, hardware, SIMD, kernel primitives |
| Tooling | testing, benchmarking, profiling, coverage |
| Cryptography | hashing, AES, RSA, key derivation |
| Scientific | linear algebra, numerical integration, statistics, DSP |
| Graphics | math3d, mesh loading, shaders, scene management |
| Filesystem | file I/O, directory walking, path manipulation |

## Tooling

| Tool | Status |
|------|--------|
| LSP server | 25 features, VS Code + Neovim + Emacs |
| Debugger | DAP-compliant, breakpoints, stepping, variable inspection |
| Build system | Incremental, dependency tracking, caching |
| Formatter | Code formatting |
| Linter | Static analysis, 15+ check categories |
| REPL | Interactive interpreter |
| Test runner | Built-in test blocks, BDD-style describe/it |
| Coverage | HTML/XML/JSON coverage reports |
| Profiler | Function-level profiling |
| Fuzzer | Fuzz testing infrastructure |
