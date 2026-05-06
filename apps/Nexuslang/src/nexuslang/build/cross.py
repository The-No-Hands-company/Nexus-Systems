"""Cross-compilation target and toolchain utilities for NexusLang."""

from __future__ import annotations

import shutil
import subprocess
from dataclasses import dataclass, field
from enum import Enum
from pathlib import Path
from typing import Dict, List, Optional, Tuple


class TargetArch(str, Enum):
    X86_64 = "x86_64"
    X86 = "x86"
    AARCH64 = "aarch64"
    ARM = "arm"
    WASM32 = "wasm32"
    RISCV32 = "riscv32"
    RISCV64 = "riscv64"

    @classmethod
    def from_str(cls, value: str) -> "TargetArch":
        normalized = value.lower()
        aliases = {
            "amd64": cls.X86_64,
            "x64": cls.X86_64,
            "arm64": cls.AARCH64,
            "i386": cls.X86,
            "i686": cls.X86,
        }
        if normalized in aliases:
            return aliases[normalized]
        for item in cls:
            if item.value == normalized:
                return item
        raise ValueError(f"Unknown architecture: {value}")


class TargetOS(str, Enum):
    LINUX = "linux"
    MACOS = "darwin"
    WINDOWS = "windows"
    NONE = "none"
    WASI = "wasi"
    UNKNOWN = "unknown"

    @classmethod
    def from_str(cls, value: str) -> "TargetOS":
        normalized = value.lower()
        aliases = {
            "macos": cls.MACOS,
            "darwin": cls.MACOS,
            "win32": cls.WINDOWS,
            "mingw32": cls.WINDOWS,
        }
        if normalized in aliases:
            return aliases[normalized]
        for item in cls:
            if item.value == normalized:
                return item
        return cls.UNKNOWN


class TargetABI(str, Enum):
    UNKNOWN = "unknown"
    NONE = "none"
    GNU = "gnu"
    GNU_EABIHF = "gnueabihf"
    MUSL = "musl"
    EABI = "eabi"
    ELF = "elf"

    @classmethod
    def from_str(cls, value: str) -> "TargetABI":
        normalized = value.lower()
        aliases = {
            "gnueabihf": cls.GNU_EABIHF,
            "gnu": cls.GNU,
            "musl": cls.MUSL,
            "eabi": cls.EABI,
            "elf": cls.ELF,
            "none": cls.NONE,
        }
        return aliases.get(normalized, cls.UNKNOWN)


@dataclass(frozen=True)
class TargetTriple:
    arch: TargetArch
    vendor: str
    os: TargetOS
    abi: TargetABI = TargetABI.UNKNOWN

    @classmethod
    def parse(cls, triple: str) -> "TargetTriple":
        parts = triple.split("-")
        if len(parts) == 4:
            arch_s, vendor, os_s, abi_s = parts
            return cls(
                arch=TargetArch.from_str(arch_s),
                vendor=vendor,
                os=TargetOS.from_str(os_s),
                abi=TargetABI.from_str(abi_s),
            )

        if len(parts) == 3:
            arch_s, second, third = parts
            arch = TargetArch.from_str(arch_s)

            third_os = TargetOS.from_str(third)
            if third_os != TargetOS.UNKNOWN:
                return cls(arch=arch, vendor=second, os=third_os, abi=TargetABI.UNKNOWN)

            if second.lower() == "none":
                return cls(
                    arch=arch,
                    vendor="none",
                    os=TargetOS.NONE,
                    abi=TargetABI.from_str(third),
                )

            return cls(
                arch=arch,
                vendor=second,
                os=TargetOS.UNKNOWN,
                abi=TargetABI.from_str(third),
            )

        raise ValueError(f"Invalid target triple: {triple}")

    @property
    def is_embedded(self) -> bool:
        return self.os == TargetOS.NONE

    @property
    def is_wasm(self) -> bool:
        return self.arch == TargetArch.WASM32

    @property
    def is_linux(self) -> bool:
        return self.os == TargetOS.LINUX

    @property
    def pointer_width(self) -> int:
        return 64 if self.arch in {TargetArch.X86_64, TargetArch.AARCH64, TargetArch.RISCV64} else 32

    def __str__(self) -> str:
        if self.vendor == "none" and self.os == TargetOS.NONE and self.abi not in {TargetABI.UNKNOWN, TargetABI.NONE}:
            return f"{self.arch.value}-none-{self.abi.value}"
        if self.abi in {TargetABI.UNKNOWN, TargetABI.NONE}:
            return f"{self.arch.value}-{self.vendor}-{self.os.value}"
        return f"{self.arch.value}-{self.vendor}-{self.os.value}-{self.abi.value}"


KNOWN_TARGETS: Dict[str, TargetTriple] = {
    "x86_64-unknown-linux-gnu": TargetTriple.parse("x86_64-unknown-linux-gnu"),
    "aarch64-unknown-linux-gnu": TargetTriple.parse("aarch64-unknown-linux-gnu"),
    "wasm32-unknown-wasi": TargetTriple.parse("wasm32-unknown-wasi"),
    "arm-none-eabi": TargetTriple.parse("arm-none-eabi"),
}


@dataclass
class ToolchainInfo:
    target: TargetTriple
    clang: Optional[Path] = None
    gcc: Optional[Path] = None
    ar: Optional[Path] = None
    ld: Optional[Path] = None
    sysroot: Optional[Path] = None

    @property
    def is_complete(self) -> bool:
        return self.compiler is not None

    @property
    def compiler(self) -> Optional[Path]:
        return self.clang or self.gcc


class ToolchainDetector:
    """Detect available toolchains for targets."""

    @staticmethod
    def _find_tool(name: str) -> Optional[Path]:
        resolved = shutil.which(name)
        return Path(resolved) if resolved else None

    def _find_sysroot(self, target: TargetTriple) -> Optional[Path]:
        if target.is_wasm:
            return None
        candidate = Path("/usr")
        return candidate if candidate.exists() else None

    def detect(self, target: TargetTriple) -> ToolchainInfo:
        triple_str = str(target)
        prefixed_gcc = self._find_tool(f"{triple_str}-gcc")
        default_gcc = self._find_tool("gcc")
        clang = self._find_tool("clang")

        return ToolchainInfo(
            target=target,
            clang=clang,
            gcc=prefixed_gcc or default_gcc,
            ar=self._find_tool(f"{triple_str}-ar") or self._find_tool("ar"),
            ld=self._find_tool(f"{triple_str}-ld") or self._find_tool("ld"),
            sysroot=self._find_sysroot(target),
        )

    def list_available(self) -> List[ToolchainInfo]:
        infos: List[ToolchainInfo] = []
        for target in KNOWN_TARGETS.values():
            info = self.detect(target)
            if info.is_complete:
                infos.append(info)
        return infos


@dataclass
class CrossCompileResult:
    success: bool
    output_file: Optional[Path] = None
    command: List[str] = field(default_factory=list)
    errors: List[str] = field(default_factory=list)
    warnings: List[str] = field(default_factory=list)


class CrossCompiler:
    """Cross-compiler driver for a detected toolchain."""

    def __init__(self, toolchain: ToolchainInfo):
        self.toolchain = toolchain

    def get_compiler_flags(self) -> List[str]:
        flags: List[str] = []
        compiler = self.toolchain.compiler

        if compiler is not None and self.toolchain.clang is not None:
            flags.append(f"--target={self.toolchain.target}")

        if self.toolchain.sysroot is not None:
            flags.append(f"--sysroot={self.toolchain.sysroot}")

        if self.toolchain.target.is_wasm or self.toolchain.target.is_embedded:
            flags.append("-nostdlib")

        return flags

    def get_linker_flags(self) -> List[str]:
        flags: List[str] = []
        if self.toolchain.target.is_wasm:
            flags.extend(["-Wl,--no-entry", "-Wl,--export-all"])
        return flags

    def compile(
        self,
        source_file: Path,
        output_file: Path,
        extra_flags: Optional[List[str]] = None,
    ) -> CrossCompileResult:
        compiler = self.toolchain.compiler
        if compiler is None:
            return CrossCompileResult(success=False, errors=["No cross-compiler available"])

        cmd = [
            str(compiler),
            *self.get_compiler_flags(),
            *(extra_flags or []),
            "-c",
            str(source_file),
            "-o",
            str(output_file),
        ]

        try:
            proc = subprocess.run(cmd, capture_output=True, text=True, check=False)
            if proc.returncode != 0:
                return CrossCompileResult(
                    success=False,
                    command=cmd,
                    errors=[proc.stderr.strip() or "Cross-compilation failed"],
                )
            return CrossCompileResult(success=True, output_file=output_file, command=cmd)
        except FileNotFoundError as exc:
            return CrossCompileResult(success=False, command=cmd, errors=[str(exc)])


def get_cross_compiler(target_triple: str) -> Tuple[CrossCompiler, ToolchainInfo]:
    target = TargetTriple.parse(target_triple)
    detector = ToolchainDetector()
    info = detector.detect(target)
    return CrossCompiler(info), info


__all__ = [
    "TargetArch",
    "TargetOS",
    "TargetABI",
    "TargetTriple",
    "ToolchainInfo",
    "ToolchainDetector",
    "CrossCompiler",
    "CrossCompileResult",
    "KNOWN_TARGETS",
    "get_cross_compiler",
]
