# Tutorial 11: Inline Assembly

**Time:** ~60 minutes  
**Prerequisites:** [FFI and C Libraries](../intermediate/04-ffi-and-c-libraries.md)

---

## Part 1 — When to Use Inline Assembly

Inline assembly is used for:

- Directly issuing CPU instructions that have no NexusLang equivalent
- Cycle-critical routines (e.g., tight inner loops, cryptographic primitives)
- Accessing hardware control registers
- Implementing synchronisation primitives (atomics, memory barriers)

It requires `--allow-asm` on the command line:

```bash
PYTHONPATH=src python -m nexuslang.main program.nlpl --allow-asm
```

Use it sparingly. Most programs do not need it.

---

## Part 2 — Basic Syntax

```nexuslang
asm
    code
        "nop"
end
```

The `asm … end` block contains assembly in Intel syntax.  Multiple
instructions are written as separate quoted strings:

```nexuslang
asm
    code
        "push rbp"
        "mov rbp, rsp"
        "pop rbp"
end
```

---

## Part 3 — Inputs and Outputs

Pass NexusLang variables into and out of the assembly block:

```nexuslang
set value to 42
set result to 0

asm
    code
        "mov rax, $1"
        "add rax, 10"
        "mov $0, rax"
    outputs "=r": result      # $0 — write to result
    inputs  "r":  value       # $1 — read from value
end

print text convert result to string    # 52
```

- `$0`, `$1`, `$2`… refer to operands in declaration order (outputs first,
  then inputs).
- Constraint `"=r"` means: output into any general-purpose register.
- Constraint `"r"` means: input from any general-purpose register.

---

## Part 4 — Clobber List

Declare registers the assembly block modifies, so the compiler can save
and restore them:

```nexuslang
set a to 5
set b to 3
set quotient  to 0
set remainder to 0

asm
    code
        "mov rax, $2"
        "xor rdx, rdx"
        "idiv $3"
        "mov $0, rax"
        "mov $1, rdx"
    outputs "=r": quotient
            "=r": remainder
    inputs  "r":  a
            "r":  b
    clobbers "rax", "rdx"
end

print text convert quotient  to string    # 1
print text convert remainder to string    # 2
```

---

## Part 5 — Memory Barriers

Use CPUID / MFENCE to enforce instruction ordering when writing lock-free
data structures:

```nexuslang
asm
    code
        "mfence"
end
```

```nexuslang
asm
    code
        "lfence"
end
```

---

## Part 6 — CPUID Example

Read CPU feature flags:

```nexuslang
set leaf    to 1
set eax_out to 0
set ecx_out to 0
set edx_out to 0
set ebx_out to 0

asm
    code
        "mov eax, $4"
        "cpuid"
        "mov $0, eax"
        "mov $1, ebx"
        "mov $2, ecx"
        "mov $3, edx"
    outputs "=r": eax_out
            "=r": ebx_out
            "=r": ecx_out
            "=r": edx_out
    inputs  "r":  leaf
    clobbers "eax", "ebx", "ecx", "edx"
end

# Bit 25 of EDX = SSE support
set has_sse to edx_out modulo 2 to the power of 26 divided by 2 to the power of 25
print text "SSE supported: " plus convert has_sse to string
```

---

## Part 7 — Volatile Assembly

Mark blocks that must not be reordered or eliminated by the compiler:

```nexuslang
asm volatile
    code
        "out 0x80, al"      # I/O port write — side-effectful
end
```

---

## Part 8 — Inline Assembly with Structs

You can reference struct fields via pointer arithmetic:

```nexuslang
struct Vector3
    x as Float
    y as Float
    z as Float
end

set v to create Vector3 with 1.0 and 2.0 and 3.0
set ptr to address of v

asm
    code
        "movss xmm0, [$1 + 0]"     # load v.x
        "addss xmm0, [$1 + 4]"     # add v.y
        "movss [$0], xmm0"          # store result
    outputs "=m": v.x
    inputs  "r":  ptr
    clobbers "xmm0"
end
```

---

## Safety Checklist

Before merging inline assembly:

- [ ] Declare all modified registers in `clobbers`
- [ ] Match output constraints to their NexusLang variables
- [ ] Use `asm volatile` for side-effectful instructions
- [ ] Test under both dev and release profiles
- [ ] Document the assembly block's purpose and the calling convention assumed

---

## Summary

| Concept | Syntax |
|---------|--------|
| Basic block | `asm … code "instr" end` |
| Input | `inputs "r": var` |
| Output | `outputs "=r": var` |
| Clobbers | `clobbers "rax", "rcx"` |
| Volatile | `asm volatile … end` |

**Next:** [Memory Management Deep Dive](02-memory-management.md)
