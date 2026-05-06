"""
Freestanding / bare-metal compilation mode for NexusLang.

When the interpreter is invoked with --freestanding, or when a linker-script
is provided, this module coordinates a minimal runtime build that:

  1. Strips all OS-dependent stdlib modules (network, subprocess, kernel, etc.).
  2. Provides a tiny bare-metal entry stub (analogous to crt0).
  3. Optionally emits a linkable LLVM IR / object file that can be linked into
     a firmware image using the supplied linker script.
  4. Reports which language features are unavailable in freestanding mode.

This module does NOT actually invoke a cross-compiler or linker itself; it
produces artefacts that can be passed to an external toolchain (e.g.,
arm-none-eabi-gcc, clang --target=..., or ld with -T <linker_script>).
"""
from __future__ import annotations

import os
import sys
import textwrap
from typing import Any, Dict, List, Optional, Set

# ---------------------------------------------------------------------------
# OS-dependent modules that must be stripped in freestanding mode
# ---------------------------------------------------------------------------

FREESTANDING_FORBIDDEN_MODULES: Set[str] = {
    "network",
    "http",
    "websocket_utils",
    "subprocess_utils",
    "kernel",
    "databases",
    "sqlite",
    "email_utils",
    "image_utils",
    "pdf_utils",
    "rendering",
    "vulkan",
    "shaders",
    "graphics",
    "scene",
    "camera",
    "mesh_loader",
    "asyncio_utils",
    "signal_utils",
    "env",
}

# Modules that are permitted (only depend on freestanding C library features)
FREESTANDING_ALLOWED_MODULES: Set[str] = {
    "math",
    "math3d",
    "string",
    "io",           # limited: no file I/O, only serial write
    "collections",
    "types",
    "type_traits",
    "algorithms",
    "bit_ops",
    "simd",
    "atomics",
    "sync",
    "threading",    # limited: only if RTOS is available
    "asm",
    "hardware",
    "interrupts",
    "allocators",
    "smart_pointers",
    "option_result",
    "limits",
    "core",
    "errno",
}

# ---------------------------------------------------------------------------
# Entry stub generation
# ---------------------------------------------------------------------------

# Minimal x86 flat-binary entry stub (NASM syntax)
_ENTRY_STUB_X86 = """\
; NexusLang freestanding entry stub – x86 (32-bit flat binary)
; Link as the very first object in your firmware image.
; The stub clears BSS, sets up a minimal stack, and calls nxl_main().
bits 32
section .text
global _start
extern nxl_main

_start:
    ; Set up stack pointer to top of reserved RAM (linker symbol)
    extern __stack_top
    mov esp, __stack_top

    ; Zero BSS section
    extern __bss_start
    extern __bss_end
    cld
    mov edi, __bss_start
    mov ecx, __bss_end
    sub ecx, edi
    xor eax, eax
    rep stosb

    ; Call NexusLang user entry point
    call nxl_main

    ; Halt on return
.halt:
    hlt
    jmp .halt
"""

# Minimal ARM Cortex-M entry stub (GNU as syntax)
_ENTRY_STUB_ARM_CORTEX_M = """\
@ NexusLang freestanding entry stub – ARM Cortex-M
@ The vector table must be placed at 0x00000000 (or as remapped).
.syntax unified
.cpu cortex-m0
.thumb

.section .vectors, "a"
.word __stack_top           @ Initial MSP
.word Reset_Handler         @ Reset vector

.section .text
.thumb_func
.global Reset_Handler
Reset_Handler:
    @ Copy .data from Flash to RAM
    ldr   r1, =__data_load
    ldr   r2, =__data_start
    ldr   r3, =__data_end
1:  cmp   r2, r3
    ittt  lt
    ldrlt r0, [r1], #4
    strlt r0, [r2], #4
    blt   1b

    @ Zero .bss
    ldr   r2, =__bss_start
    ldr   r3, =__bss_end
    movs  r0, #0
2:  cmp   r2, r3
    itt   lt
    strlt r0, [r2], #4
    blt   2b

    @ Call NexusLang main
    bl    nxl_main

    @ Spin on return
3:  b     3b
.pool
"""

# Minimal RISC-V entry stub
_ENTRY_STUB_RISCV = """\
# NexusLang freestanding entry stub – RISC-V RV32I
.section .text.start
.global _start
_start:
    # Load stack pointer
    la   sp, __stack_top

    # Zero BSS
    la   t0, __bss_start
    la   t1, __bss_end
zero_bss:
    bge  t0, t1, zero_done
    sw   zero, 0(t0)
    addi t0, t0, 4
    j    zero_bss
zero_done:

    # Call NexusLang main
    call nxl_main

halt:
    wfi
    j    halt
"""

# Generic x86-64 ELF entry (for OS kernel or UEFI bootstrap use)
_ENTRY_STUB_X86_64 = """\
; NexusLang freestanding entry stub – x86-64 ELF
bits 64
section .text
global _start
extern nxl_main

_start:
    ; Set up stack
    extern __stack_top
    mov  rsp, __stack_top
    xor  rbp, rbp

    ; Zero BSS
    extern __bss_start
    extern __bss_end
    lea  rdi, [__bss_start]
    lea  rcx, [__bss_end]
    sub  rcx, rdi
    xor  al, al
    rep  stosb

    ; Call NexusLang main
    call nxl_main

.halt:
    hlt
    jmp  .halt
"""

ARCH_STUBS: Dict[str, str] = {
    "x86":       _ENTRY_STUB_X86,
    "x86_64":    _ENTRY_STUB_X86_64,
    "arm":       _ENTRY_STUB_ARM_CORTEX_M,
    "cortex-m":  _ENTRY_STUB_ARM_CORTEX_M,
    "riscv":     _ENTRY_STUB_RISCV,
    "riscv32":   _ENTRY_STUB_RISCV,
    "riscv64":   _ENTRY_STUB_RISCV,
}


def generate_entry_stub(arch: str = "x86_64") -> str:
    """Return the assembly entry stub for the given architecture."""
    key = arch.lower()
    if key not in ARCH_STUBS:
        supported = ", ".join(sorted(ARCH_STUBS))
        raise ValueError(
            f"freestanding: unsupported architecture '{arch}'. "
            f"Supported: {supported}"
        )
    return ARCH_STUBS[key]


# ---------------------------------------------------------------------------
# Linker script templates
# ---------------------------------------------------------------------------

_LINKER_SCRIPT_X86_FLAT = """\
/* NexusLang bare-metal linker script – x86 flat binary (load at 0x00100000) */
ENTRY(_start)

MEMORY
{
    ROM  (rx)  : ORIGIN = 0x00100000, LENGTH = 512K
    RAM  (rwx) : ORIGIN = 0x00200000, LENGTH = 256K
}

SECTIONS
{
    .text : { *(.text*) } > ROM
    .rodata : { *(.rodata*) } > ROM
    .data : {
        __data_load = LOADADDR(.data);
        __data_start = .;
        *(.data*)
        __data_end = .;
    } > RAM AT > ROM
    .bss (NOLOAD) : {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        __bss_end = .;
    } > RAM
    __stack_top = ORIGIN(RAM) + LENGTH(RAM);
}
"""

_LINKER_SCRIPT_CORTEX_M = """\
/* NexusLang bare-metal linker script – ARM Cortex-M (STM32 style) */
ENTRY(Reset_Handler)

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 256K
    SRAM  (rwx) : ORIGIN = 0x20000000, LENGTH = 64K
}

SECTIONS
{
    .vectors : { KEEP(*(.vectors)) } > FLASH
    .text    : { *(.text*) *(.rodata*) } > FLASH
    .data : {
        __data_load  = LOADADDR(.data);
        __data_start = .;
        *(.data*)
        __data_end   = .;
    } > SRAM AT > FLASH
    .bss (NOLOAD) : {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        __bss_end = .;
    } > SRAM
    __stack_top = ORIGIN(SRAM) + LENGTH(SRAM);
}
"""

_LINKER_SCRIPT_RISCV = """\
/* NexusLang bare-metal linker script – RISC-V (e.g., SiFive FE310) */
ENTRY(_start)

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x20010000, LENGTH = 512K
    SRAM  (rwx) : ORIGIN = 0x80000000, LENGTH = 16K
}

SECTIONS
{
    .text.start : { KEEP(*(.text.start*)) } > FLASH
    .text       : { *(.text*) *(.rodata*) } > FLASH
    .data : {
        __data_load  = LOADADDR(.data);
        __data_start = .;
        *(.data*)
        __data_end   = .;
    } > SRAM AT > FLASH
    .bss (NOLOAD) : {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        __bss_end = .;
    } > SRAM
    __stack_top = ORIGIN(SRAM) + LENGTH(SRAM);
}
"""

LINKER_SCRIPT_TEMPLATES: Dict[str, str] = {
    "x86":      _LINKER_SCRIPT_X86_FLAT,
    "x86_64":   _LINKER_SCRIPT_X86_FLAT,
    "cortex-m": _LINKER_SCRIPT_CORTEX_M,
    "arm":      _LINKER_SCRIPT_CORTEX_M,
    "riscv":    _LINKER_SCRIPT_RISCV,
    "riscv32":  _LINKER_SCRIPT_RISCV,
    "riscv64":  _LINKER_SCRIPT_RISCV,
}


def generate_linker_script(arch: str = "x86_64") -> str:
    """Return a starter linker script for the given architecture."""
    key = arch.lower().replace("-", "_").replace("_", "-")
    if key not in LINKER_SCRIPT_TEMPLATES:
        # Try alternate key forms
        key2 = arch.lower().replace("-", "")
        for k in LINKER_SCRIPT_TEMPLATES:
            if k.replace("-", "") == key2:
                return LINKER_SCRIPT_TEMPLATES[k]
        supported = ", ".join(sorted(LINKER_SCRIPT_TEMPLATES))
        raise ValueError(
            f"freestanding: no linker script template for '{arch}'. "
            f"Supported: {supported}"
        )
    return LINKER_SCRIPT_TEMPLATES[key]


# ---------------------------------------------------------------------------
# Freestanding mode: runtime configuration
# ---------------------------------------------------------------------------

class FreestandingConfig:
    """Configuration object for freestanding / bare-metal mode.

    Created from CLI arguments and passed to the runtime before program
    execution to constrain which stdlib modules are available.
    """

    def __init__(
        self,
        arch: str = "x86_64",
        linker_script_path: Optional[str] = None,
        entry_symbol: str = "nxl_main",
        stack_size: int = 65536,
        heap_size: int = 131072,
        output_entry_stub: Optional[str] = None,
        output_linker_script: Optional[str] = None,
        enable_float: bool = True,
        enable_exceptions: bool = True,
        enable_threads: bool = False,
        verbose: bool = False,
    ) -> None:
        self.arch = arch
        self.linker_script_path = linker_script_path
        self.entry_symbol = entry_symbol
        self.stack_size = stack_size
        self.heap_size = heap_size
        self.output_entry_stub = output_entry_stub
        self.output_linker_script = output_linker_script
        self.enable_float = enable_float
        self.enable_exceptions = enable_exceptions
        self.enable_threads = enable_threads
        self.verbose = verbose

    def forbidden_modules(self) -> Set[str]:
        """Return set of module names disallowed in this configuration."""
        forbidden = set(FREESTANDING_FORBIDDEN_MODULES)
        if not self.enable_threads:
            forbidden.update({"threading", "threading_utils", "asyncio_utils", "sync"})
        return forbidden

    def validate_module_use(self, module_name: str) -> None:
        """Raise if module_name is forbidden in freestanding mode."""
        if module_name in self.forbidden_modules():
            raise FreestandingViolation(
                f"freestanding mode: module '{module_name}' requires an OS "
                f"and is not available in bare-metal builds.\n"
                f"Allowed modules: {', '.join(sorted(FREESTANDING_ALLOWED_MODULES))}"
            )

    def emit_artefacts(self) -> List[str]:
        """Write any requested output artefacts (entry stub, linker script).

        Returns list of file paths written.
        """
        written: List[str] = []

        if self.output_entry_stub:
            stub = generate_entry_stub(self.arch)
            with open(self.output_entry_stub, "w") as f:
                f.write(stub)
            written.append(self.output_entry_stub)
            if self.verbose:
                print(f"[freestanding] wrote entry stub: {self.output_entry_stub}")

        if self.output_linker_script:
            script = generate_linker_script(self.arch)
            with open(self.output_linker_script, "w") as f:
                f.write(script)
            written.append(self.output_linker_script)
            if self.verbose:
                print(f"[freestanding] wrote linker script: {self.output_linker_script}")

        return written

    def print_summary(self) -> None:
        """Print a human-readable summary of the freestanding configuration."""
        print("Freestanding / Bare-Metal Build Configuration")
        print("=" * 50)
        print(f"  Architecture    : {self.arch}")
        print(f"  Entry symbol    : {self.entry_symbol}")
        print(f"  Stack size      : {self.stack_size} bytes")
        print(f"  Heap size       : {self.heap_size} bytes")
        print(f"  Float support   : {'yes' if self.enable_float else 'no'}")
        print(f"  Exception support: {'yes' if self.enable_exceptions else 'no'}")
        print(f"  Thread support  : {'yes' if self.enable_threads else 'no'}")
        if self.linker_script_path:
            print(f"  Linker script   : {self.linker_script_path}")
        else:
            print("  Linker script   : (template – use --linker-script to supply custom)")
        forbidden = sorted(self.forbidden_modules())
        print(f"  Forbidden modules ({len(forbidden)}): {', '.join(forbidden)}")


class FreestandingViolation(Exception):
    """Raised when freestanding code uses a forbidden OS feature."""


# ---------------------------------------------------------------------------
# Integration with interpreter
# ---------------------------------------------------------------------------

def apply_freestanding_config(runtime: Any, config: FreestandingConfig) -> None:
    """Attach the freestanding config to the runtime for runtime validation.

    The module loader will call config.validate_module_use() before importing
    any module in freestanding mode.
    """
    runtime.freestanding_config = config

    # Emit any requested artefacts immediately (before interpretation)
    config.emit_artefacts()

    if config.verbose:
        config.print_summary()


def parse_freestanding_args(args: Any) -> Optional[FreestandingConfig]:
    """Build a FreestandingConfig from parsed argparse args.

    Returns None if freestanding mode is not active.
    """
    if not getattr(args, "freestanding", False):
        return None

    return FreestandingConfig(
        arch=getattr(args, "arch", "x86_64") or "x86_64",
        linker_script_path=getattr(args, "linker_script", None),
        entry_symbol=getattr(args, "entry_symbol", "nxl_main") or "nxl_main",
        stack_size=int(getattr(args, "stack_size", 65536) or 65536),
        heap_size=int(getattr(args, "heap_size", 131072) or 131072),
        output_entry_stub=getattr(args, "emit_entry_stub", None),
        output_linker_script=getattr(args, "emit_linker_script", None),
        enable_float=not getattr(args, "no_float", False),
        enable_exceptions=not getattr(args, "no_exceptions", False),
        enable_threads=getattr(args, "bare_metal_threads", False),
        verbose=getattr(args, "debug", False),
    )
