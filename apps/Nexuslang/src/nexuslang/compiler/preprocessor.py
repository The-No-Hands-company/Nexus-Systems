"""
Conditional compilation / platform detection for NexusLang.

Provides:
  - CompileTarget  : immutable value object describing the compilation target
  - detect_host()  : auto-build a CompileTarget from the current machine
  - evaluate_condition(condition_type, condition_value, target) -> bool
  - preprocess_ast(program_node, target) -> Program
    Strips branches whose conditions evaluate to False so the interpreter
    never sees dead code.

Condition types
---------------
  target_os      : "linux", "windows", "macos", "freebsd", "android", "ios"
  target_arch    : "x86", "x86_64", "arm", "arm64", "aarch64",
                   "riscv32", "riscv64", "mips", "wasm32"
  target_endian  : "little", "big"
  target_ptr_width : "32", "64"
  platform       : alias for target_os (kept for compatibility)
  feature        : named feature flags passed via --feature CLI flag

Syntax (NLPL source)
--------------------
  when target os is "linux"
      print text "running on Linux"
  end

  when target arch is "x86_64"
      print text "64-bit x86"
  otherwise
      print text "not x86_64"
  end

  when feature "networking"
      import nxl_net
  end
"""
from __future__ import annotations

import os
import sys
import platform
import struct
from dataclasses import dataclass, field
from typing import Any, Dict, List, Optional, Set


# ---------------------------------------------------------------------------
# CompileTarget
# ---------------------------------------------------------------------------

@dataclass(frozen=True)
class CompileTarget:
    """Describes the target platform for a compilation session."""

    # e.g. "linux", "windows", "macos", "freebsd"
    os: str = "linux"
    # e.g. "x86_64", "arm64", "riscv64", "wasm32"
    arch: str = "x86_64"
    # "little" or "big"
    endian: str = "little"
    # "32" or "64"
    pointer_width: str = "64"
    # Named feature flags enabled for this build
    features: frozenset = field(default_factory=frozenset)

    @property
    def is_64bit(self) -> bool:
        return self.pointer_width == "64"

    @property
    def is_little_endian(self) -> bool:
        return self.endian == "little"

    def has_feature(self, name: str) -> bool:
        return name.lower() in self.features

    def with_features(self, *names: str) -> "CompileTarget":
        return CompileTarget(
            os=self.os,
            arch=self.arch,
            endian=self.endian,
            pointer_width=self.pointer_width,
            features=frozenset(list(self.features) + [n.lower() for n in names]),
        )

    # ------------------------------------------------------------------
    # Target triple support  (x86_64-unknown-linux-gnu style)
    # ------------------------------------------------------------------

    def to_triple(self) -> str:
        """Return an LLVM-style target triple string for this target.

        Format: ``<arch>-<vendor>-<os>[-<abi>]``

        Examples::

            x86_64-pc-linux-gnu
            aarch64-unknown-linux-gnu
            riscv64-unknown-none-elf   (bare-metal)
            mips-unknown-linux-gnu
        """
        _OS_TRIPLE: Dict[str, str] = {
            "linux": "linux-gnu",
            "windows": "windows-msvc",
            "macos": "apple-darwin",
            "freebsd": "freebsd",
            "openbsd": "openbsd",
            "netbsd": "netbsd",
            "android": "linux-android",
            "ios": "apple-ios",
            "wasi": "wasi",
            "unknown": "none-elf",
        }
        vendor = "pc" if self.arch in ("x86_64", "x86") else "unknown"
        os_abi = _OS_TRIPLE.get(self.os, f"{self.os}")
        return f"{self.arch}-{vendor}-{os_abi}"

    @classmethod
    def from_triple(cls, triple: str) -> "CompileTarget":
        """Construct a CompileTarget from an LLVM-style target triple.

        Accepts 3- or 4-component triples of the form
        ``arch-vendor-os[-abi]``.  Common examples::

            x86_64-unknown-linux-gnu
            aarch64-unknown-linux-gnu
            arm-unknown-linux-gnueabihf
            riscv64-unknown-none-elf
            riscv32-unknown-none-elf
            mips-unknown-linux-gnu
            mips64-unknown-linux-gnu
            wasm32-unknown-unknown

        Raises ValueError for triples with fewer than 3 components.
        """
        parts = triple.strip().split("-")
        if len(parts) < 3:
            raise ValueError(
                f"Invalid target triple {triple!r}: need at least 3 components "
                f"(arch-vendor-os), got {len(parts)}"
            )

        raw_arch = parts[0].lower()
        # parts[1] is vendor (ignored for CompileTarget)
        raw_os = parts[2].lower()
        # parts[3] is optional ABI (ignored for CompileTarget)

        arch = _ARCH_ALIASES.get(raw_arch, raw_arch)

        _TRIPLE_OS_MAP: Dict[str, str] = {
            "linux": "linux",
            "windows": "windows",
            "apple": "macos",
            "darwin": "macos",
            "freebsd": "freebsd",
            "openbsd": "openbsd",
            "netbsd": "netbsd",
            "android": "android",
            "ios": "ios",
            "wasi": "wasi",
            "none": "unknown",
            "unknown": "unknown",
        }
        tgt_os = _TRIPLE_OS_MAP.get(raw_os, raw_os)

        # Endianness: MIPS BE by default; MIPSEL/MIPS64EL are LE
        _BIG_ENDIAN_ARCHS = {"mips", "mips64", "powerpc", "s390x"}
        endian = "big" if arch.rstrip("64") in _BIG_ENDIAN_ARCHS else "little"
        # mipsel / mips64el are explicitly little-endian
        if "el" in raw_arch:
            endian = "little"

        pointer_width = "64" if arch in (
            "x86_64", "aarch64", "riscv64", "mips64",
            "powerpc64", "powerpc64le", "s390x", "wasm64",
        ) else "32"

        return cls(
            os=tgt_os,
            arch=arch,
            endian=endian,
            pointer_width=pointer_width,
        )


def detect_host() -> CompileTarget:
    """Build a CompileTarget that reflects the host machine at runtime."""
    raw_os = platform.system().lower()
    os_map = {
        "linux": "linux",
        "windows": "windows",
        "darwin": "macos",
        "freebsd": "freebsd",
        "openbsd": "openbsd",
        "netbsd": "netbsd",
        "android": "android",
    }
    tgt_os = os_map.get(raw_os, raw_os)

    raw_machine = platform.machine().lower()
    arch_map = {
        "x86_64": "x86_64",
        "amd64": "x86_64",
        "i386": "x86",
        "i686": "x86",
        "aarch64": "aarch64",
        "arm64": "aarch64",
        "armv7l": "arm",
        "armv6l": "arm",
        "riscv64": "riscv64",
        "riscv32": "riscv32",
        "mips": "mips",
        "mips64": "mips64",
        "powerpc64le": "powerpc64le",
        "ppc64le": "powerpc64le",
        "s390x": "s390x",
    }
    tgt_arch = arch_map.get(raw_machine, raw_machine)

    endian = "big" if sys.byteorder == "big" else "little"
    ptr_width = "64" if struct.calcsize("P") == 8 else "32"

    return CompileTarget(
        os=tgt_os,
        arch=tgt_arch,
        endian=endian,
        pointer_width=ptr_width,
    )


# Singleton host target; created lazily to avoid import-time side-effects
_HOST_TARGET: Optional[CompileTarget] = None


def host_target() -> CompileTarget:
    """Return (and cache) the detected host CompileTarget."""
    global _HOST_TARGET
    if _HOST_TARGET is None:
        _HOST_TARGET = detect_host()
    return _HOST_TARGET


# ---------------------------------------------------------------------------
# Condition evaluation
# ---------------------------------------------------------------------------

# Normalisation maps so that user-supplied values are case-insensitive and
# accept common aliases.
_OS_ALIASES: Dict[str, str] = {
    "linux": "linux",
    "ubuntu": "linux",
    "debian": "linux",
    "fedora": "linux",
    "arch": "linux",
    "windows": "windows",
    "win": "windows",
    "win32": "windows",
    "win64": "windows",
    "macos": "macos",
    "mac": "macos",
    "osx": "macos",
    "darwin": "macos",
    "freebsd": "freebsd",
    "openbsd": "openbsd",
    "netbsd": "netbsd",
    "android": "android",
    "ios": "ios",
    "wasi": "wasi",
    "unknown": "unknown",
}

_ARCH_ALIASES: Dict[str, str] = {
    "x86_64": "x86_64",
    "amd64": "x86_64",
    "x64": "x86_64",
    "x86": "x86",
    "i386": "x86",
    "i686": "x86",
    "ia32": "x86",
    "arm64": "aarch64",
    "aarch64": "aarch64",
    "arm": "arm",
    "armv7": "arm",
    "armv6": "arm",
    "cortex_m": "arm",
    "cortex-m": "arm",
    "riscv64": "riscv64",
    "riscv32": "riscv32",
    "riscv": "riscv64",
    "mips": "mips",
    "mips64": "mips64",
    "wasm32": "wasm32",
    "wasm": "wasm32",
    "wasm64": "wasm64",
    "powerpc64le": "powerpc64le",
    "ppc64le": "powerpc64le",
}


def _norm_os(value: str) -> str:
    return _OS_ALIASES.get(value.lower(), value.lower())


def _norm_arch(value: str) -> str:
    return _ARCH_ALIASES.get(value.lower(), value.lower())


def evaluate_condition(
    condition_type: str,
    condition_value: str,
    target: Optional[CompileTarget] = None,
) -> bool:
    """
    Evaluate a single conditional compilation condition.

    Parameters
    ----------
    condition_type:
        One of: "target_os", "target_arch", "target_endian",
        "target_ptr_width", "platform", "feature"
    condition_value:
        The string to match.
    target:
        CompileTarget to evaluate against; defaults to host_target().

    Returns True if the condition holds.
    """
    if target is None:
        target = host_target()

    ct = condition_type.lower().replace(" ", "_")

    if ct in ("target_os", "os", "platform"):
        return target.os == _norm_os(condition_value)

    if ct in ("target_arch", "arch"):
        return target.arch == _norm_arch(condition_value)

    if ct in ("target_endian", "endian"):
        return target.endian == condition_value.lower()

    if ct in ("target_ptr_width", "ptr_width", "pointer_width"):
        return target.pointer_width == str(condition_value)

    if ct == "feature":
        return target.has_feature(condition_value)

    if ct in ("target_triple", "triple"):
        # Accept either exact triple match or arch-only prefix match.
        # e.g. condition_value "x86_64-unknown-linux-gnu" matches target with arch=x86_64, os=linux.
        # condition_value "riscv64" is treated as an arch check for convenience.
        if "-" in condition_value:
            # Full or partial triple comparison against target.to_triple()
            return target.to_triple().startswith(condition_value.lower())
        # Bare arch token: delegate to arch check
        return target.arch == _norm_arch(condition_value)

    # Unknown condition type: conservatively return False
    return False


# ---------------------------------------------------------------------------
# AST pre-processing pass: strip dead conditional blocks before interpretation
# ---------------------------------------------------------------------------

def preprocess_ast(program_node: Any,
                   target: Optional[CompileTarget] = None) -> Any:
    """
    Walk *program_node* (a ``Program`` AST node whose ``.statements`` is a
    list of statement nodes) and replace every
    ``ConditionalCompilationBlock`` with whichever branch evaluates to
    True (or nothing if neither branch exists for a False condition).

    The replacement is done *in-place* on the statements list so the same
    node object is returned for convenience.
    """
    from nexuslang.parser.ast import ConditionalCompilationBlock  # local import

    if target is None:
        target = host_target()

    def _flatten(stmts: List[Any]) -> List[Any]:
        result: List[Any] = []
        for stmt in stmts:
            if isinstance(stmt, ConditionalCompilationBlock):
                cond = evaluate_condition(
                    stmt.condition_type,
                    str(stmt.condition_value),
                    target,
                )
                chosen = stmt.body if cond else (stmt.else_body or [])
                # Recursively flatten nested conditionals
                result.extend(_flatten(chosen))
            else:
                result.append(stmt)
        return result

    if hasattr(program_node, "statements"):
        program_node.statements = _flatten(program_node.statements)
    return program_node


# ---------------------------------------------------------------------------
# Human-readable target description
# ---------------------------------------------------------------------------

def target_summary(target: Optional[CompileTarget] = None) -> str:
    """Return a one-line human-readable description of *target*."""
    if target is None:
        target = host_target()
    feat_str = (", ".join(sorted(target.features))
                if target.features else "none")
    return (
        f"os={target.os}  arch={target.arch}  "
        f"endian={target.endian}  ptr_width={target.pointer_width}  "
        f"features=[{feat_str}]"
    )
