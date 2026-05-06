"""
Native JIT Compiler (Tier-2 LLVM Backend)
==========================================

Compiles individual NexusLang functions to native machine code using the existing
LLVMIRGenerator backend.  The compiled function is loaded as a shared library
via ctypes and wrapped in a Python callable.

This replaces the previous pseudo-JIT that emitted Python source and ran it
through Python's compile() / exec() -- which only produces Python bytecode,
not native machine code.

Architecture
------------

For each hot function:

  1. Generate a standalone LLVM IR module containing:
       - The target function
       - All callee functions it depends on (recursively resolved from the
         interpreter's function registry)
       - Required runtime declarations (printf, malloc, etc.)

  2. Run opt --passes=default<O3>,coro-early,... for middle-end optimization.

  3. Run llc -filetype=obj to produce a native object file.

  4. Link to a .so shared library with clang -shared.

  5. Load the .so via ctypes.CDLL and extract the function symbol.

  6. Wrap the ctypes function in a Python callable that converts Python
     values to the appropriate C types and back.

Limitations
-----------

- Only integer-returning functions are supported in the initial implementation
  (i64 return type).  Float support is straightforward to add.

- String-returning functions require a more complex ABI (pointer + length)
  and are delegated back to the Python bytecode JIT tier for now.

- Functions that call stdlib builtins (print, len, etc.) that require the
  full interpreter runtime are not compiled natively; they fall back to the
  Python bytecode tier.

- The shared library cache is process-scoped (not persistent across runs).
  A persistent cache keyed by source hash would reduce startup latency for
  warm restarts.

Thread Safety
-------------

NativeFunctionCache is protected by a threading.Lock.  Multiple threads may
trigger compilation for different functions concurrently.  Reentrant
compilation of the same function is serialised by the per-function lock
in NativeFunctionJIT._compile().
"""

from __future__ import annotations

import ctypes
import hashlib
import os
import shutil
import subprocess
import tempfile
import threading
from typing import Any, Callable, Dict, Optional, Set

__all__ = ["NativeFunctionJIT", "NativeCompileError"]


class NativeCompileError(Exception):
    """Raised when native JIT compilation of a function fails."""


# ---------------------------------------------------------------------------
# Internal helpers
# ---------------------------------------------------------------------------

def _find_tool(name: str) -> Optional[str]:
    """Return the absolute path of an LLVM tool, or None if not found."""
    path = shutil.which(name)
    if path:
        return path
    # Fedora/RHEL use versioned names
    for suffix in ("-21", "-20", "-19", "-18", "-17", "-16", "-15", "-14"):
        versioned = shutil.which(f"{name}{suffix}")
        if versioned:
            return versioned
    return None


def _tools_available() -> bool:
    """Return True if the minimum required LLVM tools are present."""
    return bool(_find_tool("opt") and _find_tool("llc") and _find_tool("clang"))


# ---------------------------------------------------------------------------
# Shared library cache
# ---------------------------------------------------------------------------

class NativeFunctionCache:
    """
    Thread-safe cache mapping function keys to loaded ctypes callables.

    A function key encodes the function name and a hash of its IR so that
    recompilation occurs automatically when the source changes.
    """

    def __init__(self) -> None:
        self._lock = threading.Lock()
        self._cache: Dict[str, Callable] = {}
        # Keep references to loaded CDLL objects so they are not GC'd.
        self._libs: Dict[str, ctypes.CDLL] = {}
        # Temporary directory for .o / .so files
        self._tmpdir = tempfile.mkdtemp(prefix="nxl_native_jit_")

    def get(self, key: str) -> Optional[Callable]:
        with self._lock:
            return self._cache.get(key)

    def put(self, key: str, fn: Callable, lib: ctypes.CDLL) -> None:
        with self._lock:
            self._cache[key] = fn
            self._libs[key] = lib

    def tmpdir(self) -> str:
        return self._tmpdir

    def __del__(self) -> None:
        # Best-effort cleanup; ignore errors at interpreter shutdown.
        try:
            import shutil as _shutil
            _shutil.rmtree(self._tmpdir, ignore_errors=True)
        except Exception:
            pass


# Module-level singleton so all NativeFunctionJIT instances share one cache.
_global_cache = NativeFunctionCache()


# ---------------------------------------------------------------------------
# Main JIT class
# ---------------------------------------------------------------------------

class NativeFunctionJIT:
    """
    Compiles a single NexusLang function to native machine code and caches it.

    Usage::

        jit = NativeFunctionJIT(interpreter)
        callable_or_none = jit.compile("fibonacci", func_def_node)
        if callable_or_none:
            result = callable_or_none(10)  # -> int

    The returned callable accepts and returns Python ints or floats
    depending on the function signature.  If compilation fails for any
    reason, None is returned and the caller should fall back to the
    Python-bytecode JIT tier.
    """

    # Types supported for native compilation
    _SUPPORTED_TYPES = frozenset({"Integer", "Float", "Boolean"})

    def __init__(self, interpreter: Any, opt_level: int = 3) -> None:
        """
        Args:
            interpreter: The NexusLang interpreter instance.  Used to resolve
                         callee functions during IR generation.
            opt_level:   LLVM optimization level 0-3.  Default 3.
        """
        self._interpreter = interpreter
        self._opt_level = opt_level
        self._available = _tools_available()
        self._compile_locks: Dict[str, threading.Lock] = {}
        self._cl_registry_lock = threading.Lock()

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------

    @property
    def available(self) -> bool:
        """True if LLVM tools are present and native JIT is possible."""
        return self._available

    def compile(
        self,
        func_name: str,
        func_def: Any,
    ) -> Optional[Callable]:
        """
        Compile func_def and return a native callable, or None on failure.

        Results are cached; subsequent calls for the same function return
        the cached callable without recompilation.
        """
        if not self._available:
            return None

        ir_key = self._ir_key(func_name, func_def)
        cached = _global_cache.get(ir_key)
        if cached is not None:
            return cached

        # Serialise concurrent compilation of the same function.
        with self._cl_registry_lock:
            if ir_key not in self._compile_locks:
                self._compile_locks[ir_key] = threading.Lock()
        per_fn_lock = self._compile_locks[ir_key]

        with per_fn_lock:
            # Re-check cache after acquiring lock.
            cached = _global_cache.get(ir_key)
            if cached is not None:
                return cached

            try:
                return self._compile(func_name, func_def, ir_key)
            except NativeCompileError:
                return None
            except Exception:
                return None

    # ------------------------------------------------------------------
    # Internal compilation pipeline
    # ------------------------------------------------------------------

    def _compile(
        self,
        func_name: str,
        func_def: Any,
        ir_key: str,
    ) -> Optional[Callable]:
        """Run the full LLVM compilation pipeline for one function."""
        # Step 1: Generate LLVM IR for the function (and its callees)
        ir_text = self._generate_ir(func_name, func_def)
        if not ir_text:
            raise NativeCompileError(f"IR generation failed for '{func_name}'")

        tmpdir = _global_cache.tmpdir()
        base = os.path.join(tmpdir, f"fn_{func_name}_{ir_key[:8]}")
        ll_file = base + ".ll"
        opt_ll_file = base + "_opt.ll"
        obj_file = base + ".o"
        so_file = base + ".so"

        # Write IR to disk
        with open(ll_file, "w") as f:
            f.write(ir_text)

        # Step 2: Run LLVM middle-end optimization
        opt = _find_tool("opt")
        if opt:
            if self._opt_level > 0:
                passes = (
                    f"default<O{self._opt_level}>,"
                    "coro-early,coro-split,coro-elide,coro-cleanup"
                )
            else:
                passes = "coro-early,coro-split,coro-elide,coro-cleanup"
            r = subprocess.run(
                [opt, f"--passes={passes}", ll_file, "-S", "-o", opt_ll_file],
                capture_output=True, text=True,
            )
            ir_to_compile = opt_ll_file if r.returncode == 0 else ll_file
        else:
            ir_to_compile = ll_file

        # Step 3: Compile to object file
        llc = _find_tool("llc")
        if not llc:
            raise NativeCompileError("llc not found")
        llc_cmd = [llc, "-filetype=obj", "-relocation-model=pic", ir_to_compile, "-o", obj_file]
        if self._opt_level > 0:
            llc_cmd.append(f"-O{self._opt_level}")
        r = subprocess.run(llc_cmd, capture_output=True, text=True)
        if r.returncode != 0:
            raise NativeCompileError(
                f"llc failed for '{func_name}':\n{r.stderr[:800]}"
            )

        # Step 4: Link to shared library
        clang = _find_tool("clang")
        if not clang:
            raise NativeCompileError("clang not found")
        r = subprocess.run(
            [clang, "-shared", "-fPIC", obj_file, "-o", so_file, "-lm", "-lstdc++"],
            capture_output=True, text=True,
        )
        if r.returncode != 0:
            raise NativeCompileError(
                f"linking failed for '{func_name}':\n{r.stderr[:800]}"
            )

        # Step 5: Load the shared library and extract the symbol
        try:
            lib = ctypes.CDLL(so_file)
        except OSError as e:
            raise NativeCompileError(f"dlopen failed for '{func_name}': {e}")

        # The exported symbol name is "nxl_<func_name>" when we generate IR
        # for JIT use (see _generate_ir below).
        symbol_name = f"nxl_{func_name}"
        try:
            native_fn = getattr(lib, symbol_name)
        except AttributeError:
            raise NativeCompileError(
                f"Symbol '{symbol_name}' not found in compiled library"
            )

        # Step 6: Configure ctypes argument and return types
        wrapper = self._make_wrapper(func_def, native_fn)
        if wrapper is None:
            raise NativeCompileError(
                f"Cannot create ctypes wrapper for '{func_name}': unsupported types"
            )

        _global_cache.put(ir_key, wrapper, lib)
        return wrapper

    # ------------------------------------------------------------------
    # IR generation for a single function
    # ------------------------------------------------------------------

    def _generate_ir(self, func_name: str, func_def: Any) -> Optional[str]:
        """
        Generate a minimal LLVM IR module for func_def and its callees.

        Returns the IR text, or None on failure.
        """
        try:
            from ..compiler.backends.llvm_ir_generator import LLVMIRGenerator
            from ..parser.ast import Program
        except ImportError:
            return None

        # Collect the function and all recursively-called NexusLang functions
        # so the IR module is self-contained.
        func_defs = self._collect_callees(func_name, func_def)
        if not func_defs:
            return None

        # Build a minimal AST Program containing only these functions
        program = Program(statements=list(func_defs.values()))

        gen = LLVMIRGenerator()
        try:
            gen.generate(program)
        except Exception:
            return None

        ir_lines = gen.ir_lines
        ir_text = "\n".join(ir_lines)

        # Rename every reference of the form "@func_name" (followed by "("
        # or whitespace) to "@nxl_func_name" so the compiled shared library
        # exports the symbol under a predictable, non-conflicting name without
        # needing an alias whose type signature would have to be hand-crafted.
        #
        # A regex word-boundary is not used because LLVM IR identifiers may
        # contain characters not covered by \b; instead we match the token
        # boundary characters that always follow a function reference in IR:
        # "(" for call/define/declare, or whitespace for attributes/metadata.
        import re as _re
        ir_text = _re.sub(
            r'@' + _re.escape(func_name) + r'(?=[\s(])',
            '@nxl_' + func_name,
            ir_text,
        )

        return ir_text

    def _collect_callees(
        self,
        root_name: str,
        root_def: Any,
    ) -> Dict[str, Any]:
        """
        Return a dict of {name: func_def} for root_def and all NexusLang
        functions it (transitively) calls.

        Only collects functions that are defined in the interpreter's
        function registry (not stdlib builtins).
        """
        result: Dict[str, Any] = {}
        visited: Set[str] = set()
        queue = [(root_name, root_def)]

        while queue:
            name, defn = queue.pop()
            if name in visited:
                continue
            visited.add(name)
            result[name] = defn

            # Walk the function body to find callee names
            for callee_name in self._find_callees_in_body(defn):
                if callee_name in visited:
                    continue
                callee_def = self._lookup_function(callee_name)
                if callee_def is not None:
                    queue.append((callee_name, callee_def))

        return result

    def _find_callees_in_body(self, func_def: Any) -> Set[str]:
        """
        Recursively walk the function's AST body and collect all
        function call targets that are plain identifiers.
        """
        from ..parser.ast import FunctionCall
        names: Set[str] = set()

        def walk(node: Any) -> None:
            if node is None:
                return
            if isinstance(node, FunctionCall):
                if isinstance(node.name, str):
                    names.add(node.name)
                elif hasattr(node.name, "name"):
                    names.add(node.name.name)
            # Recurse into children
            for attr in vars(node) if hasattr(node, "__dict__") else []:
                val = getattr(node, attr, None)
                if val is None:
                    continue
                if hasattr(val, "__dict__"):
                    walk(val)
                elif isinstance(val, list):
                    for item in val:
                        if hasattr(item, "__dict__"):
                            walk(item)

        try:
            body = getattr(func_def, "body", [])
            for stmt in body:
                walk(stmt)
        except Exception:
            pass

        return names

    def _lookup_function(self, name: str) -> Optional[Any]:
        """Look up a function definition in the interpreter's registry."""
        if self._interpreter is None:
            return None
        try:
            # Try different locations where the interpreter stores functions
            for attr in ("functions", "global_scope", "_global_scope"):
                registry = getattr(self._interpreter, attr, None)
                if registry and isinstance(registry, dict) and name in registry:
                    return registry[name]
        except Exception:
            pass
        return None

    # ------------------------------------------------------------------
    # ctypes wrapper factory
    # ------------------------------------------------------------------

    def _make_wrapper(
        self,
        func_def: Any,
        native_fn: Any,
    ) -> Optional[Callable]:
        """
        Build a Python callable that bridges Python values to/from the
        native function's C ABI.

        Only integer-parameter, integer-returning functions are currently
        supported.  Returns None if the function uses unsupported types.
        """
        # Inspect parameter types
        params = getattr(func_def, "parameters", []) or []
        return_type = getattr(func_def, "return_type", None)
        return_type_name = (
            return_type if isinstance(return_type, str)
            else getattr(return_type, "name", "") if return_type else ""
        )

        # Determine ctypes types for each parameter
        ctypes_argtypes = []
        for p in params:
            p_type = getattr(p, "type_annotation", None)
            p_type_name = (
                p_type if isinstance(p_type, str)
                else getattr(p_type, "name", "") if p_type else ""
            )
            if p_type_name in ("Integer", "int", ""):
                ctypes_argtypes.append(ctypes.c_int64)
            elif p_type_name in ("Float", "float"):
                # Float support: not yet implemented in IR generator
                return None
            elif p_type_name in ("Boolean", "bool"):
                ctypes_argtypes.append(ctypes.c_int64)
            else:
                # Unsupported type (String, List, etc.) — cannot inline
                return None

        # Determine return type
        if return_type_name in ("Integer", "int", "", "Boolean", "bool"):
            ctypes_restype = ctypes.c_int64
        elif return_type_name in ("Float", "float"):
            return None  # Float return not yet implemented
        else:
            return None

        native_fn.argtypes = ctypes_argtypes
        native_fn.restype = ctypes_restype

        def _wrapper(*args: Any) -> Any:
            c_args = []
            for arg, ctype in zip(args, ctypes_argtypes):
                if ctype is ctypes.c_int64:
                    c_args.append(int(arg))
                else:
                    c_args.append(arg)
            result = native_fn(*c_args)
            if ctypes_restype is ctypes.c_int64:
                return int(result)
            return result

        return _wrapper

    # ------------------------------------------------------------------
    # Cache key
    # ------------------------------------------------------------------

    @staticmethod
    def _ir_key(func_name: str, func_def: Any) -> str:
        """
        Compute a stable cache key from the function name and a hash of
        the AST (via its repr).  Functions with the same name but different
        bodies will get different keys and will be recompiled.
        """
        try:
            body_repr = repr(getattr(func_def, "body", ""))
        except Exception:
            body_repr = func_name
        digest = hashlib.md5(f"{func_name}:{body_repr}".encode()).hexdigest()
        return digest
