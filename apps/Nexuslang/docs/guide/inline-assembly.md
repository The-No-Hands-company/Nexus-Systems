# Inline Assembly in NexusLang

**Status:** ✅ Fully implemented (February 2, 2026)  
**Complexity:** Advanced  
**Target Architecture:** x86_64 (AMD64)

---

## Overview

Inline assembly allows you to embed raw assembly instructions directly in NexusLang code. This provides:

- **Hardware control:** Direct CPU instruction access
- **Performance optimization:** Hand-tuned critical sections
- **System programming:** OS kernels, device drivers, bootloaders
- **Low-level operations:** Atomic instructions, SIMD, CPU features

NLPL's inline assembly features:
- x86_64 assembly syntax (Intel/AT&T styles supported)
- Register allocation and constraints
- Input/output operands
- Memory clobbers and side effects
- Integration with NexusLang variables and types

**Warning:** Inline assembly is architecture-specific and bypasses NLPL's safety checks. Use with caution!

---

## Basic Syntax

### Simple Assembly Block

```nexuslang
asm "
  ; Assembly instructions here
  mov rax, 42
  ret
"
```

**Components:**
- `asm` keyword starts the assembly block
- String literal contains assembly code
- Instructions use x86_64 assembly syntax
- Semicolon (`;`) for comments
- Must end with double quote

### Assembly with Return Value

```nexuslang
function get_value returns Integer
  asm "
    mov rax, 42
    ret
  "
end

set x to get_value
# x = 42
```

**Key points:**
- Assembly block can return values via registers
- Return value in `rax` register (following x86_64 calling convention)
- Function return type must match assembly output

---

## Register Usage

### x86_64 Register Set

**General Purpose Registers (64-bit):**
- `rax`, `rbx`, `rcx`, `rdx` - Accumulator, base, counter, data
- `rsi`, `rdi` - Source/destination index (common for string ops)
- `rbp`, `rsp` - Base pointer, stack pointer
- `r8` - `r15` - Additional general purpose

**32-bit versions:** `eax`, `ebx`, `ecx`, `edx`, `esi`, `edi`, `ebp`, `esp`, `r8d`-`r15d`  
**16-bit versions:** `ax`, `bx`, `cx`, `dx`, `si`, `di`, `bp`, `sp`, `r8w`-`r15w`  
**8-bit versions:** `al`, `bl`, `cl`, `dl`, `sil`, `dil`, `bpl`, `spl`, `r8b`-`r15b`

**Special Registers:**
- `rip` - Instruction pointer
- `rflags` - CPU flags (carry, zero, sign, overflow, etc.)

**SIMD Registers:**
- `xmm0` - `xmm15` - 128-bit SSE registers
- `ymm0` - `ymm15` - 256-bit AVX registers
- `zmm0` - `zmm31` - 512-bit AVX-512 registers (if available)

### Calling Convention (System V AMD64 ABI)

**Function Arguments (integer/pointer):**
1. `rdi` - First argument
2. `rsi` - Second argument
3. `rdx` - Third argument
4. `rcx` - Fourth argument
5. `r8` - Fifth argument
6. `r9` - Sixth argument
7. Stack - Additional arguments

**Function Arguments (floating-point):**
1. `xmm0` - First float/double
2. `xmm1` - Second float/double
3. `xmm2`-`xmm7` - Additional FP arguments

**Return Values:**
- `rax` - Integer/pointer return value
- `xmm0` - Floating-point return value
- `rdx:rax` - 128-bit return (e.g., divmod)

**Preserved Registers (callee-saved):**
- `rbx`, `rbp`, `r12`, `r13`, `r14`, `r15`
- Must be saved/restored if modified

**Scratch Registers (caller-saved):**
- `rax`, `rcx`, `rdx`, `rsi`, `rdi`, `r8`, `r9`, `r10`, `r11`
- Can be freely modified

---

## Assembly Instructions

### Data Movement

```nexuslang
function copy_value with x as Integer returns Integer
  asm "
    ; rdi contains x (first argument)
    mov rax, rdi     ; Copy x to return register
    ret
  "
end
```

**Common instructions:**
- `mov dest, src` - Move data
- `movq` - Move quadword (64-bit)
- `movl` - Move longword (32-bit)
- `movw` - Move word (16-bit)
- `movb` - Move byte (8-bit)
- `lea dest, [address]` - Load effective address
- `push src` - Push onto stack
- `pop dest` - Pop from stack

### Arithmetic Operations

```nexuslang
function add_numbers with a as Integer, b as Integer returns Integer
  asm "
    ; rdi = a, rsi = b
    mov rax, rdi     ; rax = a
    add rax, rsi     ; rax = a + b
    ret
  "
end

function multiply_by_two with x as Integer returns Integer
  asm "
    mov rax, rdi
    shl rax, 1       ; Shift left = multiply by 2
    ret
  "
end
```

**Arithmetic instructions:**
- `add dest, src` - Addition
- `sub dest, src` - Subtraction
- `imul dest, src` - Signed multiplication
- `mul src` - Unsigned multiplication (result in rdx:rax)
- `idiv src` - Signed division (rax = quotient, rdx = remainder)
- `div src` - Unsigned division
- `inc dest` - Increment
- `dec dest` - Decrement
- `neg dest` - Negate (two's complement)

### Bitwise Operations

```nexuslang
function bitwise_and with a as Integer, b as Integer returns Integer
  asm "
    mov rax, rdi
    and rax, rsi     ; rax = a & b
    ret
  "
end

function bitwise_or with a as Integer, b as Integer returns Integer
  asm "
    mov rax, rdi
    or rax, rsi      ; rax = a | b
    ret
  "
end

function bitwise_xor with a as Integer, b as Integer returns Integer
  asm "
    mov rax, rdi
    xor rax, rsi     ; rax = a ^ b
    ret
  "
end

function bitwise_not with x as Integer returns Integer
  asm "
    mov rax, rdi
    not rax          ; rax = ~x (bitwise complement)
    ret
  "
end
```

**Bitwise instructions:**
- `and dest, src` - Bitwise AND
- `or dest, src` - Bitwise OR
- `xor dest, src` - Bitwise XOR
- `not dest` - Bitwise NOT
- `shl dest, count` - Shift left (logical)
- `shr dest, count` - Shift right (logical)
- `sal dest, count` - Shift arithmetic left
- `sar dest, count` - Shift arithmetic right
- `rol dest, count` - Rotate left
- `ror dest, count` - Rotate right

### Control Flow

```nexuslang
function absolute_value with x as Integer returns Integer
  asm "
    mov rax, rdi
    test rax, rax    ; Test if negative
    jns positive     ; Jump if not sign (>= 0)
    neg rax          ; Negate if negative
  positive:
    ret
  "
end

function max with a as Integer, b as Integer returns Integer
  asm "
    mov rax, rdi
    cmp rax, rsi     ; Compare a and b
    jge done         ; Jump if a >= b
    mov rax, rsi     ; Otherwise use b
  done:
    ret
  "
end
```

**Control flow instructions:**
- `jmp label` - Unconditional jump
- `je/jz label` - Jump if equal/zero
- `jne/jnz label` - Jump if not equal/not zero
- `jg/jnle label` - Jump if greater (signed)
- `jge/jnl label` - Jump if greater or equal (signed)
- `jl/jnge label` - Jump if less (signed)
- `jle/jng label` - Jump if less or equal (signed)
- `ja label` - Jump if above (unsigned)
- `jb label` - Jump if below (unsigned)
- `jns label` - Jump if not sign (positive)
- `js label` - Jump if sign (negative)
- `call func` - Call function
- `ret` - Return from function

### Comparison and Testing

```nexuslang
function is_zero with x as Integer returns Boolean
  asm "
    xor rax, rax     ; rax = 0 (false)
    test rdi, rdi    ; Test x
    jnz done         ; Jump if not zero
    inc rax          ; rax = 1 (true)
  done:
    ret
  "
end
```

**Comparison instructions:**
- `cmp dest, src` - Compare (sets flags)
- `test dest, src` - Bitwise AND (sets flags, doesn't modify operands)

---

## Memory Operations

### Direct Memory Access

```nexuslang
function read_memory with address as Pointer returns Integer
  asm "
    mov rax, [rdi]   ; Read from address
    ret
  "
end

function write_memory with address as Pointer, value as Integer
  asm "
    mov [rdi], rsi   ; Write value to address
    ret
  "
end
```

**Memory addressing modes:**
- `[reg]` - Direct addressing (read from register as address)
- `[reg + offset]` - Displacement addressing
- `[base + index * scale]` - Scaled index addressing
- `[rip + offset]` - RIP-relative addressing

### Stack Operations

```nexuslang
function use_stack returns Integer
  asm "
    push rbp         ; Save base pointer
    mov rbp, rsp     ; Set up stack frame
    
    sub rsp, 16      ; Allocate 16 bytes on stack
    mov qword [rbp-8], 42   ; Store value
    mov rax, [rbp-8]        ; Load value
    
    mov rsp, rbp     ; Restore stack pointer
    pop rbp          ; Restore base pointer
    ret
  "
end
```

**Stack instructions:**
- `push src` - Push value onto stack (decrements rsp)
- `pop dest` - Pop value from stack (increments rsp)
- `call` - Push return address and jump
- `ret` - Pop return address and jump

---

## SIMD Operations

### SSE (Streaming SIMD Extensions)

```nexuslang
function add_floats_simd with a as Float, b as Float returns Float
  asm "
    ; xmm0 = a, xmm1 = b (floating-point arguments)
    addss xmm0, xmm1  ; Add scalar single-precision
    ; Result in xmm0
    ret
  "
end

function add_four_floats with vec1 as Pointer, vec2 as Pointer returns Pointer
  asm "
    ; Load 4 floats from vec1 into xmm0
    movaps xmm0, [rdi]
    ; Load 4 floats from vec2 into xmm1
    movaps xmm1, [rsi]
    ; Add all 4 in parallel
    addps xmm0, xmm1
    ; Return pointer to result (would need proper handling)
    mov rax, rdi
    movaps [rdi], xmm0
    ret
  "
end
```

**SIMD instructions:**
- `movaps` - Move aligned packed single-precision
- `movups` - Move unaligned packed single-precision
- `addps` - Add packed single-precision (4 floats at once)
- `addpd` - Add packed double-precision (2 doubles at once)
- `mulps` - Multiply packed single-precision
- `subps` - Subtract packed single-precision
- `divps` - Divide packed single-precision
- `addss` - Add scalar single-precision
- `addsd` - Add scalar double-precision

### AVX (Advanced Vector Extensions)

```nexuslang
function add_eight_floats with vec1 as Pointer, vec2 as Pointer
  asm "
    ; Load 8 floats from vec1 into ymm0 (256-bit)
    vmovaps ymm0, [rdi]
    ; Load 8 floats from vec2 into ymm1
    vmovaps ymm1, [rsi]
    ; Add all 8 in parallel
    vaddps ymm0, ymm0, ymm1
    ; Store result
    vmovaps [rdi], ymm0
    ret
  "
end
```

**AVX features:**
- 256-bit operations (ymm registers)
- Non-destructive 3-operand syntax
- Prefix: `v` (e.g., `vaddps` instead of `addps`)

---

## Advanced Techniques

### Atomic Operations

```nexuslang
function atomic_increment with ptr as Pointer returns Integer
  asm "
    mov rax, 1
    lock xadd [rdi], rax  ; Atomic exchange and add
    add rax, 1            ; Return old value + 1
    ret
  "
end

function compare_and_swap with ptr as Pointer, old as Integer, new as Integer returns Boolean
  asm "
    mov rax, rsi          ; Expected value
    lock cmpxchg [rdi], rdx  ; Compare and exchange
    setz al               ; Set al to 1 if equal (success)
    movzx rax, al         ; Zero-extend to 64-bit
    ret
  "
end
```

**Atomic instructions:**
- `lock` prefix - Makes instruction atomic
- `xadd` - Exchange and add
- `cmpxchg` - Compare and exchange
- `xchg` - Exchange (implicitly atomic)

### CPU Feature Detection

```nexuslang
function has_sse42 returns Boolean
  asm "
    push rbx          ; Save rbx (callee-saved)
    mov eax, 1        ; CPUID function 1
    cpuid             ; Query CPU features
    shr ecx, 20       ; Shift to SSE4.2 bit
    and ecx, 1        ; Mask bit
    mov rax, rcx      ; Return result
    pop rbx           ; Restore rbx
    ret
  "
end

function get_cpu_vendor returns String
  asm "
    ; CPUID leaves vendor string in ebx, edx, ecx
    push rbx
    xor eax, eax      ; CPUID function 0
    cpuid
    ; Would need to construct string from ebx, edx, ecx
    ; (Simplified - real implementation needs memory allocation)
    pop rbx
    ret
  "
end
```

**CPUID instruction:**
- Query CPU capabilities and features
- Function number in `eax`
- Results in `eax`, `ebx`, `ecx`, `edx`

### System Calls (Linux)

```nexuslang
function syscall_write with fd as Integer, buf as Pointer, count as Integer returns Integer
  asm "
    mov rax, 1        ; sys_write
    ; rdi = fd (already set)
    ; rsi = buf (already set)
    ; rdx = count (already set)
    syscall           ; Invoke kernel
    ; rax contains return value
    ret
  "
end

function syscall_exit with code as Integer
  asm "
    mov rax, 60       ; sys_exit
    ; rdi = code (already set)
    syscall
    ; Does not return
  "
end
```

**System call convention (Linux x86_64):**
- Syscall number in `rax`
- Arguments in `rdi`, `rsi`, `rdx`, `r10`, `r8`, `r9`
- `syscall` instruction invokes kernel
- Return value in `rax`

---

## Integration with NexusLang

### Passing NexusLang Variables

```nexuslang
function compute with x as Integer, y as Integer returns Integer
  # NexusLang variables are automatically mapped to calling convention
  asm "
    ; x is in rdi, y is in rsi
    mov rax, rdi
    imul rax, rsi    ; rax = x * y
    add rax, rdi     ; rax = x * y + x
    ret
  "
end

set result to compute with 5, 10
# result = 5 * 10 + 5 = 55
```

**Automatic mapping:**
- Integer/pointer parameters → `rdi`, `rsi`, `rdx`, `rcx`, `r8`, `r9`
- Floating-point parameters → `xmm0`-`xmm7`
- Return values → `rax` (int/pointer) or `xmm0` (float)

### Working with Structs

```nexuslang
struct Point
  x as Integer
  y as Integer
end

function get_x with p as Pointer to Point returns Integer
  asm "
    mov rax, [rdi]    ; x is at offset 0
    ret
  "
end

function get_y with p as Pointer to Point returns Integer
  asm "
    mov rax, [rdi + 8]  ; y is at offset 8 (after x)
    ret
  "
end

function set_point with p as Pointer to Point, x as Integer, y as Integer
  asm "
    mov [rdi], rsi      ; Set x
    mov [rdi + 8], rdx  ; Set y
    ret
  "
end
```

### Calling NexusLang Functions from Assembly

```nexuslang
function nxl_helper with x as Integer returns Integer
  return x times 2 plus 1
end

function use_helper with x as Integer returns Integer
  asm "
    push rdi           ; Save argument
    call nxl_helper   ; Call NexusLang function
    pop rdi            ; Restore
    add rax, rdi       ; Add original x to result
    ret
  "
end
```

---

## Safety and Best Practices

### 1. Preserve Callee-Saved Registers

```nexuslang
# Good: Save and restore rbx
function safe_function with x as Integer returns Integer
  asm "
    push rbx           ; Save rbx
    mov rbx, rdi       ; Use rbx
    ; ... computation ...
    mov rax, rbx       ; Result
    pop rbx            ; Restore rbx
    ret
  "
end

# Bad: Corrupts rbx (undefined behavior!)
function unsafe_function with x as Integer returns Integer
  asm "
    mov rbx, rdi       ; Corrupts rbx!
    ; ... computation ...
    mov rax, rbx
    ret
  "
end
```

**Must preserve:** `rbx`, `rbp`, `r12`, `r13`, `r14`, `r15`

### 2. Align Stack Properly

```nexuslang
function with_stack_frame returns Integer
  asm "
    push rbp
    mov rbp, rsp
    ; Stack must be 16-byte aligned before calls!
    sub rsp, 16       ; Allocate space (maintains alignment)
    
    ; ... use stack ...
    
    mov rsp, rbp      ; Restore
    pop rbp
    ret
  "
end
```

**Stack alignment:**
- Must be 16-byte aligned before `call` instruction
- `push`/`pop` affect alignment (8 bytes each)
- Use `sub rsp, N` where N is multiple of 16

### 3. Handle Flags Carefully

```nexuslang
# Good: Don't rely on flag state
function safe_check with x as Integer returns Boolean
  asm "
    xor rax, rax      ; Clear result
    test rdi, rdi     ; Set flags
    setz al           ; al = 1 if zero
    ret
  "
end

# Bad: Flags may be clobbered
function unsafe_check with x as Integer returns Boolean
  asm "
    test rdi, rdi     ; Set flags
    ; ... other code that may modify flags ...
    setz al           ; Wrong! Flags may have changed
    ret
  "
end
```

### 4. Document Assembly Code

```nexuslang
function complex_operation with a as Integer, b as Integer returns Integer
  asm "
    ; Purpose: Compute (a * b) + (a / b)
    ; Input: rdi = a, rsi = b
    ; Output: rax = result
    ; Modifies: rax, rdx
    
    push rbx          ; Save callee-saved register
    mov rbx, rdi      ; rbx = a
    mov rax, rdi      ; rax = a
    imul rax, rsi     ; rax = a * b
    push rax          ; Save product
    
    mov rax, rbx      ; rax = a
    xor rdx, rdx      ; Clear rdx for division
    idiv rsi          ; rax = a / b, rdx = a % b
    
    pop rbx           ; rbx = a * b
    add rax, rbx      ; rax = (a * b) + (a / b)
    pop rbx           ; Restore rbx
    ret
  "
end
```

### 5. Validate Inputs

```nexuslang
function safe_divide with a as Integer, b as Integer returns Integer
  # Check for division by zero in NexusLang
  if b equals 0
    raise error "Division by zero"
  end
  
  asm "
    mov rax, rdi
    xor rdx, rdx
    idiv rsi
    ret
  "
end
```

### 6. Use Appropriate Instruction Size

```nexuslang
# Good: Match data size
function byte_operation with x as Integer returns Integer
  asm "
    movzx rax, byte [rdi]  ; Zero-extend byte to 64-bit
    ret
  "
end

# Bad: Size mismatch
function wrong_size with x as Integer returns Integer
  asm "
    mov al, [rdi]     ; Only loads 1 byte into al
    ret               ; rax contains garbage in upper 56 bits!
  "
end
```

---

## Performance Considerations

### When to Use Inline Assembly

**Good use cases:**
- ✅ Atomic operations for thread safety
- ✅ SIMD vectorization for data processing
- ✅ System calls (if FFI unavailable)
- ✅ Hardware-specific features (CPUID, MSRs)
- ✅ System programming and low-level control
- ✅ Critical inner loops (after profiling!)

**Bad use cases:**
- ❌ Simple arithmetic (compiler optimizes better)
- ❌ Standard library operations
- ❌ Premature optimization
- ❌ Cross-platform code (assembly is architecture-specific)

### Optimization Tips

1. **Use SIMD for parallel data:**
```nexuslang
# Process 4 floats at once instead of 1
function add_arrays_simd with a as Pointer, b as Pointer, len as Integer
  asm "
    xor rcx, rcx      ; i = 0
  loop_start:
    cmp rcx, rdx      ; i < len?
    jge loop_end
    
    movaps xmm0, [rdi + rcx*4]  ; Load 4 floats from a
    movaps xmm1, [rsi + rcx*4]  ; Load 4 floats from b
    addps xmm0, xmm1            ; Add all 4 in parallel
    movaps [rdi + rcx*4], xmm0  ; Store result
    
    add rcx, 4        ; i += 4 (processed 4 elements)
    jmp loop_start
  loop_end:
    ret
  "
end
```

2. **Reduce memory accesses:**
```nexuslang
# Good: Keep values in registers
function sum_array with arr as Pointer, len as Integer returns Integer
  asm "
    xor rax, rax      ; sum = 0
    xor rcx, rcx      ; i = 0
  loop:
    cmp rcx, rsi
    jge done
    add rax, [rdi + rcx*8]  ; sum += arr[i]
    inc rcx
    jmp loop
  done:
    ret
  "
end
```

3. **Use lea for arithmetic:**
```nexuslang
function multiply_by_five with x as Integer returns Integer
  asm "
    lea rax, [rdi + rdi*4]  ; rax = x + x*4 = x*5 (faster than imul!)
    ret
  "
end
```

4. **Minimize branches:**
```nexuslang
# Good: Branchless absolute value
function abs_branchless with x as Integer returns Integer
  asm "
    mov rax, rdi
    mov rdx, rax
    sar rdx, 63       ; rdx = sign extension (all 1s if negative)
    xor rax, rdx      ; Flip bits if negative
    sub rax, rdx      ; Add 1 if negative
    ret
  "
end
```

---

## Debugging Assembly Code

### Common Issues

1. **Segmentation Fault:**
   - Invalid memory access
   - Misaligned pointer
   - Stack corruption
   - Check memory addresses and alignment

2. **Wrong Results:**
   - Register misuse
   - Flag interpretation error
   - Integer overflow
   - Print intermediate values for debugging

3. **Crashes After Return:**
   - Callee-saved register not restored
   - Stack pointer corruption
   - Return address overwritten

### Debugging Techniques

```nexuslang
# Add debug output
function debug_assembly with x as Integer returns Integer
  print text "Input: " plus (x to_string)
  
  asm "
    mov rax, rdi
    add rax, 42
    ret
  "
  
  # Can't print inside asm block, but can wrap it
end

# Use try/catch for safety
function safe_asm_call with x as Integer returns Integer
  try
    asm "
      ; Potentially dangerous code
      mov rax, [rdi]
      ret
    "
  catch e
    print text "Assembly error: " plus e
    return 0
  end
end
```

---

## Examples

### Example 1: Fast Bit Count

```nexuslang
function count_bits with x as Integer returns Integer
  asm "
    popcnt rax, rdi   ; Count set bits (requires SSE4.2)
    ret
  "
end

set bits to count_bits with 0b11010110
# bits = 5
```

### Example 2: Swap Without Temporary

```nexuslang
function xor_swap with a as Pointer, b as Pointer
  asm "
    mov rax, [rdi]    ; Load *a
    mov rbx, [rsi]    ; Load *b
    xor rax, rbx      ; a ^= b
    xor rbx, rax      ; b ^= a (original a)
    xor rax, rbx      ; a ^= b (original b)
    mov [rdi], rax    ; Store to *a
    mov [rsi], rbx    ; Store to *b
    ret
  "
end
```

### Example 3: Memory Barrier

```nexuslang
function memory_barrier
  asm "
    mfence            ; Full memory fence
    ret
  "
end

function load_barrier
  asm "
    lfence            ; Load fence
    ret
  "
end

function store_barrier
  asm "
    sfence            ; Store fence
    ret
  "
end
```

---

## Further Reading

- **FFI Guide:** `docs/reference/stdlib/ffi.md`
- **Memory Management:** `docs/tutorials/advanced/02-memory-management.md`
- **Struct/Union Types:** `docs/guide/structs-and-unions.md`
- **Language Reference:** `docs/reference/language-spec.md`
- **x86_64 Reference:** Intel/AMD architecture manuals
- **System V ABI:** Calling convention specification

---

## Summary

Inline assembly in NexusLang provides:

✅ **Direct hardware access** - CPU instructions, registers, flags  
✅ **x86_64 support** - Full instruction set (SSE, AVX, atomic ops)  
✅ **NLPL integration** - Automatic parameter/return value mapping  
✅ **Performance** - Hand-tuned critical code paths  
✅ **System programming** - OS kernel, driver, bootloader development  

**Use inline assembly when:**
- You need maximum performance in critical sections
- You're writing OS/system code
- You need hardware-specific features
- You understand the trade-offs (portability, safety, maintenance)

**Avoid inline assembly when:**
- NexusLang or standard library provides the functionality
- You haven't profiled to identify bottlenecks
- You need cross-platform code
- Compiler optimizations are sufficient

**Status:** Production-ready for system programming and low-level optimization!
