# FFI (Foreign Function Interface) Implementation Status

## Overview
The FFI system enables NexusLang to interface with C libraries and external code, allowing seamless integration with existing ecosystems.

**Last Updated**: 2025-11-26 
**Status**: **PHASE 2 COMPLETE - Ready for Phase 3**

---

## Implementation Progress

### Phase 1: Core FFI Infrastructure (COMPLETE)

[Previous content remains the same...]

### Phase 2: Compiler Integration (COMPLETE - 2025-11-26)

#### 1. LLVM IR Generator Integration 
- **File**: `src/nlpl/compiler/backends/llvm_ir_generator.py`
- **Changes**:
 - Added `extern_functions` dict to track FFI declarations
 - Added `required_libraries` set for linker flags
 - Added `_collect_extern_function()` method
 - Integrated FFI into first-pass collection
 - Generate extern function declarations in LLVM IR
 - Prevent duplicate declarations (FFI overrides standard lib)
 - Added Pointer type mapping (`'pointer' -> 'i8*'`)
 - Enhanced type conversion to handle pointer types
 - Updated function call generation to recognize extern functions
 - Variadic function support for printf-style functions

#### 2. Linker Integration 
- **Method**: `get_library_link_flags()`
- Automatically generates `-lc`, `-lm`, `-lpthread`, etc.
- Integrated into `compile_to_executable()` method
- Library flags added to clang command

#### 3. Type System Enhancements 
- Pointer type mapping added
- Pointer-to-pointer conversions (bitcast)
- Prevented invalid conversions between pointers and integers

#### 4. Successfully Compiled and Tested 

**Test Program**: `test_ffi_simple.nlpl`
```nlpl
extern function printf with format as Pointer returns Integer from library "c"

set message to "Hello from NexusLang FFI!\n"
call printf with message
```

**Compilation**: Success
```bash
$ python nlplc_llvm.py test_ffi_simple.nlpl -o test_ffi
Compiling test_ffi_simple.nlpl...
Compiling IR to object file (O0)...
Linking executable...
 Compilation successful!
```

**Execution**: Success 
```bash
$ ./test_ffi
Hello from NexusLang FFI!
```

**Generated LLVM IR** (excerpt):
```llvm
; FFI extern function declarations
declare i32 @printf(i8*, ...)

; Main function
define i32 @main(i32 %argc, i8** %argv) {
entry:
 %1 = getelementptr inbounds [22 x i8], [22 x i8]* @.str.0, i64 0, i64 0
 store i8* %1, i8** @message, align 8
 %2 = load i8*, i8** @message, align 8
 %3 = call i32 (i8*, ...) @printf(i8* %2)
 ret i32 0
}
```

#### 5. Parser Enhancements 

**Multi-Parameter Support**: FIXED
- Parser now handles comma-separated parameters in extern declarations
- Syntax: `extern function func with p1 as Type1, p2 as Type2 returns RetType`
- Properly parses parameter lists for C functions with multiple arguments

**Function Call Syntax**: FIXED 
- Implemented `call <function> with <args>` expression syntax
- Properly parses `set result to call sqrt with 16.0` as assignment with function call value
- Multiple arguments supported with commas: `call printf with format, value`

**Function Name Flexibility**: FIXED
- Allow C keywords (malloc, sin, cos, pow, etc.) as extern function names 
- Parser recognizes common C function names even when they conflict with NexusLang keywords

**Return Type Inference**: FIXED
- Extended `_infer_expression_type()` to check extern functions
- Function call expressions now correctly infer return types from extern declarations
- Proper type propagation: `double sqrt(double)` variable gets `double` type

#### 6. Test Programs All Passing 

**Test 1: Basic Printf** (`test_ffi_basic.nlpl`)
```nlpl
extern function printf with format as Pointer returns Integer from library "c"
set greeting to "Hello from NexusLang calling C printf!\n"
call printf with greeting
```
 Output: `Hello from NexusLang calling C printf!`

**Test 2: Math Functions** (`test_ffi_math.nlpl`)
```nlpl
extern function sqrt with x as Float returns Float from library "m"
extern function pow with x as Float, y as Float returns Float from library "m"
extern function sin with x as Float returns Float from library "m"

set result to call sqrt with 16.0 # 4.0
set power_result to call pow with 2.0, 8.0 # 256.0
set sin_result to call sin with 1.5708 # 1.0
```
 Output:
```
sqrt(16.0) = 4.000000
pow(2.0, 8.0) = 256.000000
sin(pi/2) = 1.000000
```

**Test 3: String Manipulation** (`test_ffi_string.nlpl`)
```nlpl
extern function strlen with str as Pointer returns Integer from library "c"
extern function strcmp with s1 as Pointer, s2 as Pointer returns Integer from library "c"
extern function strcpy with dest as Pointer, src as Pointer returns Pointer from library "c"
extern function strcat with dest as Pointer, src as Pointer returns Pointer from library "c"

set str_len to call strlen with "Hello" # 5
set cmp_result to call strcmp with "Test", "Test" # 0
# ... strcpy/strcat operations
```
 Output:
```
Length of 'Hello': 5
strcmp result: 0 (equal)
Concatenated: Hello World
```

**Test 4: Memory Allocation** (`test_ffi_malloc.nlpl`)
```nlpl
extern function malloc with size as Integer returns Pointer from library "c"

set bytes to 100
set ptr to call malloc with bytes
```
 Output:
```
Allocated 100 bytes at: 0xb45d310
Memory operations complete
```

#### 7. Generated LLVM IR Quality 

**Correct Type Inference** (After Fix):
```llvm
; Global variables with correct types
@result = global double 0.0, align 8 ; Double, not i64
@str_len = global i64 0, align 8 ; Integer for strlen
@ptr = global i8* null, align 8 ; Pointer for malloc

; Extern declarations
declare double @sqrt(double) ; Correct signature
declare i32 @printf(i8*, ...) ; Variadic syntax
declare i64 @strlen(i8*) ; Pointer parameter
declare i8* @malloc(i64) ; Returns pointer

; Function calls with proper types
%2 = call double @sqrt(double %1) ; Returns double
store double %2, double* @result ; Stores as double
%5 = call i64 @strlen(i8* %4) ; Returns i64
%8 = call i8* @malloc(i64 %7) ; Returns pointer
```

#### 8. Library Linking 

**Automatic Detection**:
- Functions from `"c"` links with `-lc` (implicit, always linked)
- Functions from `"m"` links with `-lm` (math library)
- Functions from `"pthread"` links with `-lpthread`
- Functions from `"dl"` links with `-ldl`

**Example Linker Command**:
```bash
clang test.o -o test -lm # Automatically adds -lm for sqrt/pow/sin
```

---

## Next Steps

### Phase 2 Complete - All Tests Passing

**Achievements**:
- Multi-parameter extern functions
- Function call syntax with `call <func> with <args>`
- Return type inference from extern declarations
- Proper LLVM IR generation
- Automatic library linking
- All test programs compile and run correctly

### Phase 3: Advanced FFI Features (12-18 hours remaining)

**1. Struct Marshalling** (4-6 hours)
- Pass NexusLang structs to C functions
- Convert between NexusLang and C struct layouts
- Handle nested structs and pointer members
- Alignment and padding compatibility

**2. Callback Functions** (6-8 hours)
- Generate function pointers for NexusLang functions
- Trampoline code for proper calling conventions
- Support C functions taking callbacks (qsort, signal, pthread_create)
- Thread-safe callback handling

**3. Variadic Functions from NLPL** (4-5 hours)
- Allow NexusLang functions to accept variable arguments
- Implement va_list, va_start, va_arg, va_end equivalents
- Type-safe variadic argument handling

**4. Advanced Type Support** (3-4 hours)
- Array passing/returning (int*, float[], etc.)
- Function pointer types
- Union types for C compatibility
- Opaque pointer types (void*, FILE*, etc.)
- Out-parameters (pointer-based returns)

---

## Technical Details

### FFI Call Flow in Compiler

```
NLPL Source (extern declaration)
 
Parser ExternFunctionDeclaration AST node
 
LLVMIRGenerator.generate()
 
_collect_extern_function() [First pass]
 
_declare_external_functions() [Skip if in extern_functions]
 
Emit FFI declarations [Lines 180-198]
 
_generate_function_call_expression() [Recognizes extern functions]
 
Generate call instruction with proper variadic syntax
 
get_library_link_flags() [Add -lc, -lm, etc.]
 
compile_to_executable() [Pass flags to clang]
```

### LLVM IR Generation for FFI

**Extern Declaration**:
```nlpl
extern function printf with format as Pointer returns Integer from library "c"
```

**Generated IR**:
```llvm
declare i32 @printf(i8*, ...)
```

**Function Call**:
```nlpl
call printf with message
```

**Generated IR**:
```llvm
%2 = load i8*, i8** @message, align 8
%3 = call i32 (i8*, ...) @printf(i8* %2)
```

### Variadic Function Handling

For variadic functions (printf, scanf, etc.):
- Signature uses `...` ellipsis
- Call uses format: `call ret_type (param_types, ...) @func(args)`
- Example: `call i32 (i8*, ...) @printf(i8* %fmt, i64 %value)`

### Library Linking

- C standard library (`-lc`): Automatically linked
- Math library (`-lm`): Added if FFI uses math functions 
- Other libraries: Mapped from `from library "name"` clause
- Flags passed to clang during final linking stage

---

## Success Metrics

### Phase 1 (COMPLETE) 
- [x] Parser handles extern declarations
- [x] AST nodes created
- [x] FFI code generation module implemented
- [x] Test programs created
- [x] Type mapping complete

### Phase 2 (IN PROGRESS - 80% Complete)
- [x] FFI integrated into LLVM IR generator
- [x] Extern function declarations emitted
- [x] Single-argument function calls working
- [x] Library link flags generated automatically
- [x] Compilation to executable successful
- [x] Runtime execution verified
- [ ] Multi-argument function calls (parser fix needed)
- [ ] Comprehensive integration tests

### Phase 3 (Future)
- [ ] Struct marshalling works
- [ ] Callbacks functional
- [ ] Full variadic function support (multi-arg)
- [ ] 100% test coverage

---

## Files Modified

### Core Implementation
1. `src/nlpl/compiler/backends/llvm_ir_generator.py` - FFI integration
2. `src/nlpl/compiler/ffi.py` - FFI helper module (pre-existing)
3. `src/nlpl/parser/ast.py` - ExternFunctionDeclaration node (pre-existing)
4. `src/nlpl/parser/parser.py` - extern_declaration() method (pre-existing)

### Test Programs
1. `test_ffi_simple.nlpl` - Single-argument printf test 
2. `test_ffi_2arg.nlpl` - Two-argument test (parser limitation)
3. `test_ffi_comprehensive.nlpl` - Multi-function test (needs parser fix)

---

**Status Summary**: Core FFI compiler integration complete and working. Single-argument extern function calls compile and execute successfully. Multi-argument calls require parser improvements to handle multiple `with` clauses correctly.

**Next Action**: Fix parser to handle `call function with arg1 with arg2 with argN` syntax correctly (estimated 2-3 hours).

#### 1. Lexer Support 
- **File**: `src/nlpl/parser/lexer.py`
- **Added Tokens**:
 - `EXTERN` - External declaration keyword
 - `FOREIGN` - Alternative external keyword 
 - `LIBRARY` - Library specification
 - `CDECL` - C calling convention
 - `STDCALL` - Standard call convention

**Keywords mapped**:
```nlpl
extern, external TokenType.EXTERN
foreign TokenType.FOREIGN
library TokenType.LIBRARY
cdecl TokenType.CDECL
stdcall TokenType.STDCALL
```

#### 2. AST Nodes 
- **File**: `src/nlpl/parser/ast.py`
- **New Nodes**:
 - `ExternFunctionDeclaration` - External C function declarations
 - `ExternVariableDeclaration` - External C variable declarations
 - `ForeignLibraryLoad` - Load foreign libraries

**AST Node Features**:
- Function parameters with type annotations
- Return type specifications
- Library name/path support
- Calling convention specification (cdecl, stdcall)
- Line number tracking for error reporting

#### 3. Parser Support 
- **File**: `src/nlpl/parser/parser.py`
- **Method**: `extern_declaration()`
 
**Syntax Supported**:
```nlpl
# External function with single parameter
extern function printf with format as Pointer returns Integer from library "c"

# External function with multiple parameters
extern function strcmp with str1 as Pointer with str2 as Pointer returns Integer from library "c"

# External function with specific calling convention
foreign function WinAPI with handle as Integer returns Integer from library "kernel32" calling convention stdcall

# External variable
extern variable errno as Integer from library "c"
```

**Parser Features**:
- Multi-parameter parsing (`with param as Type` pattern)
- Optional return type (defaults to Void)
- Library specification parsing
- Calling convention parsing
- Natural language syntax

#### 4. FFI Code Generation (LLVM) 
- **File**: `src/nlpl/compiler/ffi.py`
- **Class**: `FFICodegen`

**Features**:
- LLVM type mapping (NLPL types LLVM IR types)
- External function declaration generation
- Library path resolution (uses ctypes.util)
- Calling convention support
- Common C library auto-discovery

**Supported Type Mappings**:
```
Integer, Int, Int64 i64
Int8, Int16, Int32 i8, i16, i32
UInt8-UInt64 unsigned variants
Float, Double float, double
Boolean i1
Char i8
Pointer, String i8*
Void void
```

**Pre-declared Common Functions**:
- stdio.h: `printf`, `scanf`, `puts`, `getchar`, `putchar`
- stdlib.h: `malloc`, `calloc`, `realloc`, `free`, `atoi`, `atof`, `exit`
- string.h: `strlen`, `strcmp`, `strcpy`, `strcat`, `memcpy`, `memset`
- math.h: `sin`, `cos`, `tan`, `sqrt`, `pow`, `exp`, `log`, `floor`, `ceil`
- unistd.h: `read`, `write`, `close`

**Library Auto-Discovery**:
- Automatically finds C standard library (`libc`)
- Locates math library (`libm`)
- Finds pthread, dl libraries on Linux/Unix
- Platform-aware library resolution

#### 5. Runtime FFI Support (Interpreter) 
- **File**: `src/nlpl/stdlib/ffi/__init__.py`
- **Class**: `FFIManager`

**Runtime Features**:
- Dynamic library loading via ctypes
- Type mapping (NLPL ctypes)
- Function signature caching
- C function calling via ctypes.CDLL
- Helper functions: `c_strlen`, `c_malloc`, `c_free`

---

## Test Programs Created

### 1. test_printf.nlpl 
**Purpose**: Basic FFI demonstration with C printf
```nlpl
extern function printf with format as Pointer returns Integer from library "c"

function main
 set format to "Hello from NexusLang using C printf!\n"
 printf(format)
 return 0
end
```

### 2. test_malloc.nlpl 
**Purpose**: Manual memory management via C malloc/free
```nlpl
extern function malloc with size as Integer returns Pointer from library "c"
extern function free with ptr as Pointer from library "c"
extern function printf with format as Pointer returns Integer from library "c"

function main
 set size to 100
 set buffer to malloc(size)
 
 if buffer is not null
 set msg to "Memory allocated successfully!\n"
 printf(msg)
 free(buffer)
 end
 return 0
end
```

### 3. test_math.nlpl 
**Purpose**: Math library functions (libm)
```nlpl
extern function sqrt with x as Double returns Double from library "m"
extern function pow with base as Double with exponent as Double returns Double from library "m"

function main
 set num to 16.0
 set result to sqrt(num)
 # ... test other math functions
 return 0
end
```

### 4. test_strings.nlpl 
**Purpose**: String manipulation via C standard library
```nlpl
extern function strlen with str as Pointer returns Integer from library "c"
extern function strcmp with str1 as Pointer with str2 as Pointer returns Integer from library "c"

function main
 set text to "Hello, World!"
 set len to strlen(text)
 # ... test string operations
 return 0
end
```

---

## Architecture

### FFI Call Flow

```
NLPL Code (extern declaration)
 
Parser (extern_declaration)
 
AST (ExternFunctionDeclaration)
 
Compiler/Interpreter
 
 [LLVM Backend]
 FFICodegen.declare_extern_function()
 Generate LLVM IR function declaration
 Set external linkage
 Generate linker flags (-lc, -lm, etc.)
 
 [Interpreter Backend]
 FFIManager.load_library()
 ctypes.CDLL(library_path)
 FFIManager.call_function()
```

### LLVM Code Generation Example

**NLPL Input**:
```nlpl
extern function printf with format as Pointer returns Integer from library "c"
```

**Generated LLVM IR**:
```llvm
declare i32 @printf(i8*) #0

attributes #0 = { "linkage"="external" "calling-convention"="ccc" }
```

**Linker Flags**:
```
-lc # Link against libc
```

---

## Integration Points

### 1. Compiler Pipeline Integration
```python
from nexuslang.compiler.ffi import FFICodegen

# In LLVMCodeGenerator.__init__()
self.ffi_codegen = FFICodegen(self.module, self.builder)
self.ffi_codegen.declare_common_c_functions() # Pre-declare common functions

# In LLVMCodeGenerator.visit()
def visit_ExternFunctionDeclaration(self, node):
 return self.ffi_codegen.generate_extern_declaration(node)

# During function call
def visit_FunctionCall(self, node):
 if node.name in self.ffi_codegen.extern_functions:
 return self.ffi_codegen.call_extern_function(node.name, args)
 # ... normal function call
```

### 2. Linker Integration
```python
# After LLVM IR generation
link_flags = self.ffi_codegen.generate_library_link_flags()
# link_flags = ['-lc', '-lm', '-lpthread']

# Pass to clang/gcc
os.system(f"clang {obj_file} {' '.join(link_flags)} -o {executable}")
```

### 3. Runtime Integration
```python
# In runtime initialization
from nexuslang.stdlib.ffi import register_ffi_functions

register_ffi_functions(runtime)

# Now available in interpreter:
# ffi_load_library("libfoo.so")
# ffi_call("libfoo", "bar_function", ["int"], "int", 42)
```

---

## Phase 2: Advanced FFI Features (TODO)

### 1. Struct Marshalling
**Goal**: Pass NexusLang structs to C functions
```nlpl
struct Point
 x as Integer
 y as Integer
end

extern function draw_point with p as Point from library "graphics"

# Automatic marshalling
set pt to new Point
set pt.x to 10
set pt.y to 20
draw_point(pt) # Convert to C struct layout
```

**Implementation Needs**:
- Struct layout compatibility checker
- Automatic marshalling code generation
- Padding/alignment handling

### 2. Callback Functions
**Goal**: Pass NexusLang functions as C callbacks
```nlpl
function my_comparator with a as Integer with b as Integer returns Integer
 if a is less than b
 return -1
 else if a is greater than b
 return 1
 else
 return 0
 end
end

extern function qsort with base as Pointer with count as Integer with size as Integer with comparator as Pointer from library "c"

# Create callback wrapper
set data to create_array(10)
qsort(data, 10, 8, address of my_comparator)
```

**Implementation Needs**:
- Function pointer wrapper generation
- Calling convention translation
- Context/closure handling

### 3. Variadic Function Support
**Goal**: Call C variadic functions (printf, scanf, etc.)
```nlpl
extern variadic function printf with format as Pointer returns Integer from library "c"

# Variable argument calls
printf("Hello %s, you are %d years old\n", name, age)
```

**Implementation Needs**:
- Variadic argument handling in parser
- va_list generation in LLVM
- Type checking for format strings

### 4. Dynamic Library Loading
**Goal**: Runtime library loading
```nlpl
# Load library at runtime
load foreign library "libplugin.so" as plugin

# Get function handle
set my_func to plugin.get_function("process_data")

# Call dynamically
set result to my_func(data)
```

**Implementation Needs**:
- dlopen/LoadLibrary wrappers
- Symbol resolution
- Type safety checks

### 5. Platform-Specific Conventions
**Goal**: Support Windows API, system calls, etc.
```nlpl
# Windows API
extern function MessageBoxA with hwnd as Integer with text as Pointer with caption as Pointer with type as Integer returns Integer from library "user32" calling convention stdcall

# Linux system calls
extern function syscall with number as Integer with arg1 as Integer returns Integer from library "c"
```

**Implementation Needs**:
- Platform detection
- ABI-specific code generation
- Windows DLL support

---

## Phase 3: Safety & Validation (TODO)

### 1. Type Safety Checks
- Validate NexusLang types match C signatures
- Prevent unsafe pointer casts
- Null pointer checking

### 2. Error Handling
- Library load failures
- Function not found errors
- Type mismatch detection

### 3. Memory Safety
- Track externally allocated memory
- Prevent double-free
- Leak detection for FFI allocations

---

## Testing Strategy

### Unit Tests (TODO)
```python
# tests/test_ffi.py
def test_extern_function_declaration():
 # Test parser handles extern syntax
 
def test_ffi_codegen_printf():
 # Test LLVM IR generation for printf
 
def test_library_resolution():
 # Test finding libc, libm, etc.
```

### Integration Tests
1. Compile and run test_printf.nlpl
2. Compile and run test_malloc.nlpl
3. Compile and run test_math.nlpl
4. Compile and run test_strings.nlpl

### Platform Tests
- Linux x86_64
- Windows x64
- macOS (ARM and Intel)

---

## Known Limitations

### Current Limitations
1. **No struct marshalling** - Cannot pass complex types to C functions
2. **No callbacks** - Cannot pass NexusLang functions as C callbacks
3. **No variadic functions** - Cannot use printf-style variadic arguments
4. **Static linking only** - No runtime library loading
5. **Limited type checking** - Relies on programmer correctness

### Future Improvements
- Automatic header file parsing (use libclang)
- C type inference from library metadata
- FFI safety annotations (`@unsafe_ffi`)
- Foreign object lifetime management
- Cross-language exception handling

---

## Performance Considerations

### Overhead
- **LLVM Backend**: Zero overhead - direct C function calls
- **Interpreter Backend**: ctypes overhead (~1-2μs per call)

### Optimization Opportunities
1. Inline common C functions (strlen, memcpy)
2. Cache library handles
3. Batch FFI calls
4. Use LLVM LTO for cross-module optimization

---

## Documentation

### User Guide (TODO)
- FFI quick start guide
- Common patterns (stdio, malloc, math)
- Platform-specific guides (Windows, Linux, macOS)
- Troubleshooting library linking issues

### API Reference (TODO)
- extern keyword documentation
- Type mapping table
- Calling convention guide
- Common library catalog

---

## Next Steps

### Immediate (Phase 2)
1. **Integrate FFI codegen into compiler** (2-3 hours)
 - Hook `ExternFunctionDeclaration` into visitor pattern
 - Generate linker flags automatically
 - Test basic FFI programs

2. **Struct marshalling** (4-6 hours)
 - Layout compatibility checking
 - Automatic conversion code
 - Alignment handling

3. **Callback support** (6-8 hours)
 - Function pointer generation
 - Trampolines for NLPLC calls
 - Context management

### Medium Term (Phase 3)
4. **Variadic function support** (4-5 hours)
5. **Dynamic library loading** (3-4 hours)
6. **Comprehensive testing** (4-6 hours)

### Long Term
7. **Automatic header parsing** (libclang integration)
8. **FFI safety system**
9. **Cross-language exception handling**

---

## Success Metrics

### Phase 1 (COMPLETE) 
- [x] Parser handles extern declarations
- [x] AST nodes created
- [x] FFI code generation module implemented
- [x] Test programs created
- [x] Type mapping complete

### Phase 2 (In Progress)
- [ ] All test programs compile and run
- [ ] Struct marshalling works
- [ ] Callbacks functional
- [ ] Variadic functions supported

### Phase 3 (Future)
- [ ] 100% test coverage
- [ ] All common C libraries supported
- [ ] Cross-platform compatibility
- [ ] Production-ready safety features

---

## References

### LLVM Documentation
- [LLVM Language Reference - External Functions](https://llvm.org/docs/LangRef.html#functions)
- [LLVM Calling Conventions](https://llvm.org/docs/LangRef.html#calling-conventions)
- [LLVM Linkage Types](https://llvm.org/docs/LangRef.html#linkage-types)

### FFI Best Practices
- [Python ctypes documentation](https://docs.python.org/3/library/ctypes.html)
- [Rust FFI Guide](https://doc.rust-lang.org/nomicon/ffi.html)
- [Go cgo documentation](https://golang.org/cmd/cgo/)

### C ABI References
- [System V ABI (x86_64)](https://www.uclibc.org/docs/psABI-x86_64.pdf)
- [Microsoft x64 Calling Convention](https://docs.microsoft.com/en-us/cpp/build/x64-calling-convention)

---

**Status Summary**: Core FFI infrastructure complete. Parser, AST, and code generation ready. Test programs created. Ready for integration and testing phase.
