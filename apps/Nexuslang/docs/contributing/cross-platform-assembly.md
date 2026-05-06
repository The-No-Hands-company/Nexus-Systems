# Cross-Platform Inline Assembly Guide

## Overview

NLPL supports inline assembly across multiple architectures:
- **x86_64** (Intel/AMD 64-bit) - Fully tested ✅
- **x86** (Intel/AMD 32-bit) - Implemented, untested ⚠️
- **AArch64** (ARM 64-bit) - Implemented, requires hardware ⚠️
- **ARM** (ARM 32-bit) - Implemented, requires hardware ⚠️

This guide covers cross-platform inline assembly development, focusing on ARM/AArch64 since x86_64 is already documented.

---

## Architecture Detection

NLPL automatically detects your target architecture at compile time:

```bash
# On x86_64 Linux
python dev_tools/nlplc program.nlpl  # Compiles for x86_64

# On ARM Raspberry Pi
python dev_tools/nlplc program.nlpl  # Compiles for aarch64

# On Apple Silicon Mac
python dev_tools/nlplc program.nlpl  # Compiles for aarch64
```

The compiler uses `platform.machine()` to detect:
- `x86_64`, `AMD64` → x86_64 target
- `i386`, `i686` → x86 target
- `aarch64`, `arm64` → AArch64 target
- `armv7l`, `armv6l` → ARM target

---

## ARM/AArch64 Register Conventions

### General Purpose Registers

**AArch64 (64-bit ARM)**:
- `x0-x30`: 64-bit general purpose registers
- `w0-w30`: 32-bit views of x0-x30 (lower 32 bits)
- `xzr`/`wzr`: Zero register (reads as 0, writes discarded)
- `sp`: Stack pointer
- `lr` (`x30`): Link register (return address)
- `pc`: Program counter (not directly accessible)

**Register Usage Convention**:
- `x0-x7`: Function arguments and return values
- `x8`: Indirect result location
- `x9-x15`: Temporary registers (caller-saved)
- `x16-x17`: Intra-procedure-call scratch registers
- `x18`: Platform register (OS-specific)
- `x19-x28`: Callee-saved registers
- `x29`: Frame pointer
- `x30`: Link register

### SIMD/Floating Point Registers

- `v0-v31`: 128-bit NEON vector registers
- `q0-q31`: 128-bit views (4x float, 2x double, etc.)
- `d0-d31`: 64-bit views (1x double, 2x float, etc.)
- `s0-s31`: 32-bit views (1x float)
- `h0-h31`: 16-bit views (half-precision float)
- `b0-b31`: 8-bit views

---

## Constraint Mapping

### x86_64 Constraints

```nlpl
"r"  # Any general register (rax, rbx, rcx, rdx, rsi, rdi, r8-r15)
"a"  # rax/eax/ax/al
"b"  # rbx/ebx/bx/bl
"c"  # rcx/ecx/cx/cl
"d"  # rdx/edx/dx/dl
"S"  # rsi/esi/si/sil
"D"  # rdi/edi/di/dil
"m"  # Memory operand
"i"  # Immediate integer
```

### ARM/AArch64 Constraints

```nlpl
"r"  # Any general register (x0-x30)
"w"  # Floating point/NEON register (v0-v31)
"x"  # FP/NEON register (alternative to "w")
"m"  # Memory operand
"I"  # Immediate 12-bit unsigned (shifted)
"J"  # Immediate 0
"K"  # Immediate for logical operations
"L"  # Immediate for 64-bit MOV
```

---

## Architecture-Specific Examples

### Example 1: Addition (Cross-Platform)

**x86_64**:
```nlpl
set a to 10
set b to 20
set result to 0

asm
    code
        "mov rax, $1"
        "add rax, $2"
        "mov $0, rax"
    outputs "=r": result
    inputs "r": a, "r": b
    clobbers "rax"
end
```

**AArch64**:
```nlpl
set a to 10
set b to 20
set result to 0

asm
    code
        "add x0, x1, x2"    # x0 = x1 + x2
        "mov $0, x0"
    outputs "=r": result
    inputs "r": a, "r": b
    clobbers "x0"
end
```

### Example 2: Conditional Logic

**x86_64**:
```nlpl
set x to 10
set y to 5
set is_greater to 0

asm
    code
        "mov rax, $1"
        "cmp rax, $2"
        "setg al"
        "movzx rax, al"
        "mov $0, rax"
    outputs "=r": is_greater
    inputs "r": x, "r": y
    clobbers "rax"
end
```

**AArch64**:
```nlpl
set x to 10
set y to 5
set is_greater to 0

asm
    code
        "cmp x1, x2"
        "cset x0, gt"       # Set x0 = 1 if greater, 0 otherwise
        "mov $0, x0"
    outputs "=r": is_greater
    inputs "r": x, "r": y
    clobbers "x0"
end
```

### Example 3: Bitwise Operations

**x86_64**:
```nlpl
set val to 15
set mask to 10
set result to 0

asm
    code
        "mov rax, $1"
        "and rax, $2"
        "mov $0, rax"
    outputs "=r": result
    inputs "r": val, "r": mask
    clobbers "rax"
end
```

**AArch64**:
```nlpl
set val to 15
set mask to 10
set result to 0

asm
    code
        "and x0, x1, x2"
        "mov $0, x0"
    outputs "=r": result
    inputs "r": val, "r": mask
    clobbers "x0"
end
```

---

## SIMD Operations

### x86_64 SSE/AVX

```nlpl
# Add 4 floats using SSE
asm
    code
        "movaps xmm0, [$1]"
        "addps xmm0, [$2]"
        "movaps [$0], xmm0"
    outputs "=m": result
    inputs "m": vec_a, "m": vec_b
    clobbers "xmm0"
end
```

### ARM NEON

```nlpl
# Add 4 floats using NEON
asm
    code
        "ld1 {v0.4s}, [x1]"     # Load 4 floats
        "ld1 {v1.4s}, [x2]"
        "fadd v0.4s, v0.4s, v1.4s"  # Add vectors
        "st1 {v0.4s}, [x0]"     # Store result
    outputs "=m": result
    inputs "r": vec_a, "r": vec_b
    clobbers "v0", "v1"
end
```

---

## Testing on ARM Hardware

### Raspberry Pi (AArch64)

1. **Install NexusLang on Raspberry Pi**:
```bash
# Raspberry Pi OS (64-bit required)
git clone https://github.com/yourusername/NLPL.git
cd NexusLang
pip3 install -r requirements.txt
```

2. **Run ARM tests**:
```bash
# Basic operations
python3 dev_tools/nlplc test_programs/unit/assembly/test_asm_arm_basic.nlpl
./test_asm_arm_basic

# Advanced features
python3 dev_tools/nlplc test_programs/unit/assembly/test_asm_arm_advanced.nlpl
./test_asm_arm_advanced

# NEON SIMD
python3 dev_tools/nlplc test_programs/unit/assembly/test_asm_arm_neon.nlpl
./test_asm_arm_neon
```

3. **Expected output**:
```
=== ARM/AArch64 Inline Assembly Tests ===

Test 1: Basic ADD operation
Result: 30 (expected: 30)

Test 2: Basic SUB operation
Result: 35 (expected: 35)

...
=== All ARM basic tests completed ===
```

### Apple Silicon Mac (M1/M2/M3)

1. **Install NLPL**:
```bash
# macOS with Homebrew
brew install python3
git clone https://github.com/yourusername/NLPL.git
cd NexusLang
pip3 install -r requirements.txt
```

2. **Run tests** (same as Raspberry Pi above)

3. **Rosetta 2 Note**: Ensure you're running native ARM, not x86_64 emulation:
```bash
arch  # Should show "arm64"
```

### AWS Graviton (ARM Server)

1. **Launch Graviton instance**:
   - EC2 instance type: t4g, m6g, c6g, or r6g
   - OS: Ubuntu 20.04/22.04 ARM64

2. **Install and test** (same steps as Raspberry Pi)

---

## Common Pitfalls

### 1. Register Size Mismatches

**Wrong** (mixing 32-bit and 64-bit):
```nlpl
asm
    code
        "mov w0, x1"  # ERROR: Size mismatch
end
```

**Correct**:
```nlpl
asm
    code
        "mov x0, x1"  # 64-bit to 64-bit
        # OR
        "mov w0, w1"  # 32-bit to 32-bit
end
```

### 2. Calling Convention Violations

**Wrong** (clobbering callee-saved registers without saving):
```nlpl
asm
    code
        "mov x19, 42"  # x19 is callee-saved!
    clobbers "x19"     # But we didn't save/restore it
end
```

**Correct** (use temporary registers):
```nlpl
asm
    code
        "mov x9, 42"   # x9 is caller-saved (safe)
    clobbers "x9"
end
```

### 3. Syntax Differences

| Feature | x86_64 | AArch64 |
|---------|--------|---------|
| Order | `op dst, src` | `op dst, src1, src2` |
| Immediate prefix | None | `#` (optional) |
| Memory syntax | `[base+offset]` | `[base, offset]` or `[base, #offset]` |
| Conditional suffix | Separate instruction | Instruction suffix (e.g., `addeq`) |

---

## Debugging ARM Assembly

### 1. Disassemble compiled binary

```bash
# On ARM hardware
objdump -d test_asm_arm_basic

# Look for your inline assembly blocks
# They'll be marked with # markers or appear as raw instructions
```

### 2. Use QEMU for emulation (x86_64 host)

```bash
# Install QEMU user mode
sudo apt install qemu-user-static

# Compile for ARM on x86_64 (cross-compilation)
# Note: NexusLang doesn't support cross-compilation yet
# This requires adding --target flag in future

# Run ARM binary on x86_64
qemu-aarch64-static ./test_asm_arm_basic
```

### 3. Enable verbose output

```bash
# Compile with debug info
python dev_tools/nlplc --debug test_asm_arm_basic.nlpl

# This shows:
# - Architecture detection
# - Register validation
# - Generated LLVM IR
```

---

## Performance Considerations

### ARM-Specific Optimizations

1. **Use conditional execution**:
```nlpl
# Instead of branch
asm code "cmp x0, x1" "cset x2, gt" end

# More efficient than:
asm code "cmp x0, x1" "bgt .label" ".label: mov x2, 1" end
```

2. **Leverage load/store multiple**:
```nlpl
# Efficient multi-register load
asm code "ldp x0, x1, [x2]" end  # Load pair
```

3. **Use NEON for vectorization**:
```nlpl
# 4x speedup for float operations
asm code "fadd v0.4s, v1.4s, v2.4s" end
```

---

## Reporting Issues

If you encounter ARM-related issues, please report with:

1. **Hardware details**:
   - Device model (Raspberry Pi 4, Apple M1, etc.)
   - CPU: `cat /proc/cpuinfo` or `sysctl -a | grep cpu`
   - OS: `uname -a`

2. **NLPL version**:
   ```bash
   git rev-parse HEAD
   ```

3. **Failing test**:
   - Test file name
   - Expected vs actual output
   - Compilation errors (if any)

4. **Architecture detection**:
   ```bash
   python3 -c "import platform; print(platform.machine())"
   ```

---

## Future Enhancements

Planned improvements for ARM support:

- [ ] Cross-compilation from x86_64 to ARM
- [ ] More ARM-specific instruction validation
- [ ] NEON intrinsics library
- [ ] Apple Silicon-specific optimizations
- [ ] Raspberry Pi GPIO integration
- [ ] ARM Thumb mode support

---

## Resources

### ARM Architecture References

- [ARM Architecture Reference Manual](https://developer.arm.com/documentation/ddi0487/latest/)
- [ARM Compiler armasm User Guide](https://developer.arm.com/documentation/dui0801/latest/)
- [NEON Programmer's Guide](https://developer.arm.com/architectures/instruction-sets/simd-isas/neon/neon-programmers-guide)

### AArch64 Calling Convention

- [Procedure Call Standard for ARM 64-bit](https://github.com/ARM-software/abi-aa/blob/main/aapcs64/aapcs64.rst)

### Testing Platforms

- [Raspberry Pi](https://www.raspberrypi.org/)
- [AWS Graviton](https://aws.amazon.com/ec2/graviton/)
- [Oracle Cloud ARM instances](https://www.oracle.com/cloud/compute/arm/)

---

**Document Status**: Ready for hardware testing  
**Last Updated**: February 14, 2026  
**Tested On**: x86_64 only (ARM tests ready but untested)
