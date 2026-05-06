"""Link-Time Optimization support for NexusLang build workflows."""

from __future__ import annotations

import shutil
import subprocess
from dataclasses import dataclass, field
from enum import Enum
from pathlib import Path
from typing import Dict, List, Optional


class LTOMode(str, Enum):
    DISABLED = "disabled"
    THIN = "thin"
    FULL = "full"


@dataclass
class LTOConfig:
    mode: LTOMode = LTOMode.DISABLED
    opt_level: int = 2
    strip_debug: bool = False
    internalize: bool = True
    passes: str = ""


@dataclass
class LTOResult:
    success: bool
    output_file: Optional[Path] = None
    errors: List[str] = field(default_factory=list)
    warnings: List[str] = field(default_factory=list)
    returncode: int = 0
    stdout: str = ""
    stderr: str = ""


class LLVMTools:
    """Resolver/cacher for external LLVM tools used by LTO."""

    def __init__(self) -> None:
        self._cache: Dict[str, Optional[str]] = {}

    def find(self, tool_name: str) -> Optional[str]:
        if tool_name in self._cache:
            return self._cache[tool_name]
        found = shutil.which(tool_name)
        self._cache[tool_name] = found
        return found

    def check_lto_tools(self) -> Dict[str, Optional[str]]:
        return {
            "llvm-link": self.find("llvm-link"),
            "opt": self.find("opt"),
            "llc": self.find("llc"),
            "llvm-strip": self.find("llvm-strip"),
        }

    def all_required_available(self) -> bool:
        return all(path is not None for path in self.check_lto_tools().values())


class LTOLinker:
    """Driver for LTO linking pipeline."""

    def __init__(self, config: LTOConfig, tools: Optional[LLVMTools] = None) -> None:
        self.config = config
        self.tools = tools or LLVMTools()

    def emit_bitcode_flags(self) -> List[str]:
        if self.config.mode == LTOMode.DISABLED:
            return []
        if self.config.mode == LTOMode.THIN:
            return ["-emit-llvm", "-flto=thin"]
        return ["-emit-llvm", "-flto"]

    def link_with_lto(
        self,
        bitcode_files: List[Path],
        output: Path,
        work_dir: Optional[Path] = None,
    ) -> LTOResult:
        if not bitcode_files:
            return LTOResult(success=False, errors=["No bitcode files provided for LTO"])

        if self.config.mode == LTOMode.DISABLED:
            return LTOResult(success=True, output_file=output)

        if not self.tools.all_required_available():
            missing = [
                name for name, path in self.tools.check_lto_tools().items() if path is None
            ]
            return LTOResult(
                success=False,
                errors=[f"Missing required LLVM tools: {', '.join(missing)}"],
            )

        llvm_link = self.tools.find("llvm-link")
        opt = self.tools.find("opt")
        llc = self.tools.find("llc")

        assert llvm_link is not None
        assert opt is not None
        assert llc is not None

        work_dir = work_dir or output.parent
        work_dir.mkdir(parents=True, exist_ok=True)

        linked_bc = work_dir / "linked.bc"
        optimized_bc = work_dir / "optimized.bc"
        output_parent = output.parent
        output_parent.mkdir(parents=True, exist_ok=True)

        mode_pass = "thinlto" if self.config.mode == LTOMode.THIN else "default"
        opt_flag = f"-O{self.config.opt_level}"

        commands = [
            [llvm_link, *[str(p) for p in bitcode_files], "-o", str(linked_bc)],
            [opt, opt_flag, f"-passes={self.config.passes or mode_pass}", str(linked_bc), "-o", str(optimized_bc)],
            [llc, str(optimized_bc), "-filetype=obj", "-o", str(output)],
        ]

        for cmd in commands:
            try:
                proc = subprocess.run(cmd, capture_output=True, text=True, check=False)
            except FileNotFoundError as exc:
                return LTOResult(success=False, errors=[str(exc)])

            if proc.returncode != 0:
                err = proc.stderr.strip() or f"Command failed: {' '.join(cmd)}"
                return LTOResult(
                    success=False,
                    errors=[err],
                    returncode=proc.returncode,
                    stdout=proc.stdout,
                    stderr=proc.stderr,
                )

        return LTOResult(success=True, output_file=output)


def lto_flags_for_profile(
    mode: LTOMode,
    opt_level: int = 2,
    strip: bool = False,
) -> List[str]:
    if mode == LTOMode.DISABLED:
        return []

    flags: List[str] = []
    if mode == LTOMode.THIN:
        flags.append("-flto=thin")
    else:
        flags.append("-flto")

    flags.append(f"-O{opt_level}")
    if strip:
        flags.append("-s")

    return flags


__all__ = [
    "LTOMode",
    "LTOConfig",
    "LTOResult",
    "LLVMTools",
    "LTOLinker",
    "lto_flags_for_profile",
]
