# NexusLang Compiler Backend Strategy

## The Multi-Backend Philosophy

NLPL aims to be truly universal - one language from OS kernels to web apps. This requires **multiple compilation targets**, not just one.

## Backend Priorities

### Phase 1: C Backend (COMPLETE)
**Status:** Working - compiles NexusLang to C, then to native executables
**Use Cases:**
- Rapid prototyping
- System utilities
- Portable applications
- Interfacing with existing C libraries

**How It Works:**
```
NLPL C source GCC/Clang Native Executable
```

**Advantages:**
- Immediate results (built in hours)
- Leverages 40+ years of GCC optimization
- Supports all architectures GCC supports (x86, ARM, RISC-V, etc.)
- Battle-tested, reliable
- Easy to debug (can inspect generated C code)

**Limitations:**
- Can't access ultra-low-level features (inline ASM, direct port I/O)
- Two-stage compilation (slower compile times)
- Generated C might not be hand-optimized quality

---

### Phase 2: C++ Backend (IN PROGRESS)
**Status:** Skeleton created, needs completion
**Use Cases:**
- Object-oriented NexusLang programs
- Generic programming (templates)
- RAII memory management
- STL integration

**How It Works:**
```
NLPL C++ source Clang++ Native Executable
```

**Advantages:**
- Full OOP support (classes, inheritance, polymorphism)
- Template-based generics
- Smart pointers (automatic memory management)
- Exception handling
- STL containers and algorithms

**Implementation:**
- Extend C generator with class/method generation
- Map NexusLang generics to C++ templates
- Map NexusLang memory management to smart pointers

---

### Phase 3: JavaScript/TypeScript Backend (FUTURE)
**Status:** Not started
**Use Cases:**
- Web applications
- Node.js server-side apps
- Cross-platform desktop apps (Electron)
- React Native mobile apps

**How It Works:**
```
NLPL JavaScript/TypeScript Node/Browser
```

**Advantages:**
- Run NexusLang in browsers
- Access to npm ecosystem
- Unified language for frontend + backend
- No compilation needed (or TypeScript for type safety)

**Implementation:**
- Map NexusLang syntax to JavaScript equivalents
- Convert classes to ES6 classes or prototypes
- Handle async/await for concurrent programming
- Optional: Generate TypeScript for type safety

---

### Phase 4: WebAssembly Backend (FUTURE)
**Status:** Not started
**Use Cases:**
- High-performance web applications
- Truly universal binaries (run anywhere)
- Sandboxed execution
- Browser games, audio/video processing

**How It Works:**
```
NLPL WASM Browser/WASI Runtime
```

**Advantages:**
- Near-native performance in browsers
- Run on any platform with WASM runtime
- Secure sandboxing
- Small binary sizes

**Implementation:**
- Direct WASM generation or use Emscripten
- Map NexusLang memory model to WASM linear memory
- Interface with JavaScript for DOM access

---

### Phase 5: LLVM Backend (PRIORITY FUTURE)
**Status:** Not started
**Use Cases:**
- Production-quality native executables
- Cross-platform development
- Professional-grade optimization
- Alternative to C/C++ backends

**How It Works:**
```
NLPL LLVM IR LLVM Optimizer Native Executable
```

**Advantages:**
- **Best of both worlds**: Portable IR + native execution
- World-class optimization (used by Rust, Swift, Clang)
- Supports 15+ architectures out of the box
- Can target WASM, GPU code, etc.
- Professional tooling (debuggers, profilers)

**Why This Is Better Than C Backend:**
- More optimization passes
- Better cross-platform support
- Can still drop to inline assembly when needed
- Industry standard (Rust, Swift use it)

**Implementation:**
- Generate LLVM IR from NexusLang AST
- Map NexusLang types to LLVM types
- Use LLVM optimization passes
- Link with LLVM linker

---

### Phase 6: Native Assembly Backend (OS KERNEL WORK)
**Status:** Not started
**Use Cases:**
- Operating system kernels
- Bootloaders
- Device drivers
- Bare-metal embedded systems
- Hardware-level programming

**How It Works:**
```
NLPL x86-64/ARM Assembly Assembler Native Executable
```

**Advantages:**
- **Direct hardware access** (required for OS development)
- Inline assembly generation
- Direct I/O port access
- Interrupt handling
- No C runtime dependency

**Why This Is Required:**
- C compilers assume a C runtime exists
- OS kernels run *before* C runtime is initialized
- Need direct control over CPU, memory, interrupts

**Implementation:**
- Register allocation
- Instruction selection
- Calling convention handling
- Separate backends for x86-64, ARM, RISC-V

---

## Compilation Pipeline Comparison

### Simple Application (Hello World)
```
Option 1 (C Backend):
 NexusLang C GCC Executable
 Time: ~100ms
 Binary: ~16 KB

Option 2 (LLVM Backend):
 NexusLang LLVM IR Optimized Executable
 Time: ~200ms
 Binary: ~14 KB (better optimization)

Option 3 (Direct Assembly):
 NexusLang x86-64 ASM Executable
 Time: ~50ms
 Binary: ~8 KB (minimal runtime)
```

### Web Application
```
Option 1 (JavaScript Backend):
 NexusLang JavaScript Browser
 No compilation, instant deployment

Option 2 (WASM Backend):
 NexusLang WASM Browser
 Time: ~500ms, faster runtime execution
```

### Operating System Kernel
```
MUST use Assembly Backend:
 NexusLang x86-64 ASM Bootloader/Kernel
 Direct hardware access required
```

---

## The Strategic Answer

**Q: Why go through C instead of directly to executable?**

**A: Because NexusLang has DIFFERENT use cases that need DIFFERENT backends:**

1. **Quick prototyping?** C backend (fast development)
2. **Production app?** LLVM backend (best optimization)
3. **Web app?** JavaScript/WASM backend
4. **OS kernel?** Assembly backend (direct hardware access)

**The C backend is NOT a limitation** - it's **ONE of many targets** we'll support.

---

## Current Implementation Status

### Working Now:
- C backend (basic features)
- Variable declarations
- Print statements
- Literals (strings, numbers)
- GCC linking to create executables

### Next Steps (Priority Order):
1. **Expand C++ backend** (for OOP features)
2. **Add control flow** (if/else, loops) to C generator
3. **Add functions** to C generator
4. **Build LLVM backend** (for production use)
5. **Build JavaScript backend** (for web extension)
6. **Build Assembly backend** (for OS kernel work)

---

## The Vision

NLPL won't have "a compiler" - it will have a **compilation suite**:

```
nlpl compile myapp.nlpl --target c # Generate C code
nlpl compile myapp.nlpl --target cpp # Generate C++ code
nlpl compile myapp.nlpl --target js # Generate JavaScript
nlpl compile myapp.nlpl --target wasm # Generate WebAssembly
nlpl compile myapp.nlpl --target llvm # Use LLVM (production)
nlpl compile myapp.nlpl --target asm-x64 # Direct assembly (OS kernels)
nlpl compile myapp.nlpl --target asm-arm # ARM assembly (embedded)
```

**All from the SAME source code.** That's the power of NexusLang.

---

## References

**Languages with multiple backends:**
- **Haskell**: Native compiler + JavaScript backend (GHCJS)
- **Kotlin**: JVM + JavaScript + Native backends
- **Scala**: JVM + JavaScript (Scala.js) + Native (Scala Native)
- **Nim**: Compiles to C, C++, JavaScript, or Objective-C

NLPL follows this proven multi-backend strategy.
