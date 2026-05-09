"""Legacy compatibility surface for LLVM backend imports.

This module used to contain an older llvmlite-based implementation that had
diverged from the production LLVM backend and accumulated stale failure paths.
The real implementation now lives in ``llvm_ir_generator.py``.

The goal here is compatibility, not a second backend:
- preserve the historical import path ``nexuslang.compiler.backends.llvm_generator``
- preserve the public class name ``LLVMCodeGenerator``
- route all code generation through the maintained production generator
"""

from __future__ import annotations

from typing import Optional

from nexuslang.compiler import CompilationTarget

from .llvm_ir_generator import LLVMIRGenerator


class LLVMTypeMapper:
    """Compatibility adapter for the legacy ``map_type`` API."""

    def __init__(self):
        self._generator = LLVMIRGenerator()

    def map_type(self, nxl_type: Optional[str]) -> str:
        """Map a NexusLang type name to an LLVM IR type string.

        ``None`` keeps the legacy default of ``i64`` for callers that relied on
        the old mapper's implicit integer fallback. Named types delegate to the
        production backend so type lowering stays consistent across import paths.
        """
        if nxl_type is None:
            return "i64"
        return self._generator._map_nxl_type_to_llvm(nxl_type)


class LLVMCodeGenerator(LLVMIRGenerator):
    """Compatibility subclass preserving the legacy backend class name."""

    def __init__(self, target: str = CompilationTarget.LLVM_IR):
        super().__init__(target=target)


__all__ = ["LLVMCodeGenerator", "LLVMTypeMapper"]
