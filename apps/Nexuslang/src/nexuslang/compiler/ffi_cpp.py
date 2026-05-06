"""
C++ Interoperability for NexusLang FFI.

Implements five C++ interop features:
1. Name mangling / demangling - Itanium ABI (GCC/Clang) and MSVC ABI
2. C++ class wrapping - Expose C++ classes as NLPL-callable objects
3. Template instantiation - Declare pre-instantiated template functions/classes
4. Exception handling across FFI boundary - catch/rethrow std::exception
5. RTTI support - dynamic_cast equivalents, typeid, type_info queries

Architecture note:
  NexusLang can only call C++ code through the C ABI.  All C++ features below
  generate C-ABI wrappers (extern "C" thunks) that can be emitted as a C
  header + implementation file to be compiled by the host C++ compiler.
  The resulting object can then be linked normally.

  For LLVM-IR mode (inlined), we generate equivalent IR where the C++
  complexities are abstracted behind the thunks.
"""

import re
import struct
import textwrap
from dataclasses import dataclass, field
from enum import Enum, auto
from typing import Any, Dict, List, Optional, Tuple, Pattern


# ===========================================================================
# 1. Name Mangling / Demangling
# ===========================================================================

class ManglingABI(Enum):
    """C++ name mangling ABI variants."""
    ITANIUM = auto()    # GCC / Clang (Linux, macOS, most platforms)
    MSVC    = auto()    # Microsoft Visual C++ (Windows)


# ---------------------------------------------------------------------------
# Itanium ABI demangler (subset sufficient for NexusLang FFI use cases)
# ---------------------------------------------------------------------------

# Itanium ABI substitution table (built-in substitute codes)
_ITANIUM_SUBSTITUTIONS: Dict[str, str] = {
    "St": "std::",
    "Sa": "std::allocator",
    "Sb": "std::basic_string",
    "Ss": "std::string",
    "Si": "std::istream",
    "So": "std::ostream",
    "Sd": "std::iostream",
}

# Itanium ABI builtin type codes
_ITANIUM_BUILTINS: Dict[str, str] = {
    "v": "void",
    "b": "bool",
    "c": "char",
    "a": "signed char",
    "h": "unsigned char",
    "s": "short",
    "t": "unsigned short",
    "i": "int",
    "j": "unsigned int",
    "l": "long",
    "m": "unsigned long",
    "x": "long long",
    "y": "unsigned long long",
    "n": "__int128",
    "o": "unsigned __int128",
    "f": "float",
    "d": "double",
    "e": "long double",
    "g": "__float128",
    "z": "...",
    "Di": "char32_t",
    "Ds": "char16_t",
    "Du": "char8_t",
    "Da": "auto",
    "Dc": "decltype(auto)",
    "Dn": "std::nullptr_t",
}


class ItaniumDemangler:
    """
    Subset Itanium ABI C++ symbol demangler.

    Handles:
    - Global functions and variables
    - Member functions (including const, volatile qualifiers)
    - Constructors and destructors
    - Template instantiations (simple cases)
    - Namespace-qualified names
    - Pointer, reference, and const qualifiers
    - Standard library substitutions (std::string, etc.)
    """

    def __init__(self, mangled: str):
        self._s = mangled
        self._pos = 0
        self._substitutions: List[str] = []  # backref table for 'S' codes

    def demangle(self) -> str:
        """Entry point for demangling an Itanium-mangled symbol."""
        if not self._s.startswith("_Z"):
            return self._s   # Not mangled

        self._pos = 2  # skip "_Z"
        try:
            name = self._parse_encoding()
            return name
        except (IndexError, ValueError):
            return self._s  # Return raw on parse failure

    # ------------------------------------------------------------------
    # Internal parser helpers
    # ------------------------------------------------------------------

    def _peek(self) -> str:
        return self._s[self._pos] if self._pos < len(self._s) else ""

    def _consume(self, n: int = 1) -> str:
        chunk = self._s[self._pos: self._pos + n]
        self._pos += n
        return chunk

    def _parse_encoding(self) -> str:
        """encoding ::= <name> <bare-function-type>
                      | <special-name>"""
        if self._peek() == "T":
            return self._parse_special()
        name = self._parse_name()
        if self._pos >= len(self._s):
            return name
        # bare-function-type: the parameter types
        params = self._parse_bare_function_type()
        if params:
            return f"{name}({', '.join(params)})"
        return name

    def _parse_special(self) -> str:
        """Special names: TV (vtable), TI (typeinfo), TS (typeinfo name)."""
        self._consume()  # 'T'
        code = self._consume()
        if code == "V":
            return f"vtable for {self._parse_type()}"
        if code == "I":
            return f"typeinfo for {self._parse_type()}"
        if code == "S":
            return f"typeinfo name for {self._parse_type()}"
        return f"<special {code}>"

    def _parse_name(self) -> str:
        """name ::= <nested-name> | <unscoped-name> | <local-name>"""
        if self._peek() == "N":
            return self._parse_nested_name()
        if self._peek() == "L":
            return self._parse_local_name()
        return self._parse_unscoped_name()

    def _parse_nested_name(self) -> str:
        """nested-name ::= N [<CV-qualifiers>] <prefix> <unqualified-name> E"""
        self._consume()  # 'N'
        qualifiers = self._parse_cv_qualifiers()
        parts: List[str] = []
        while self._peek() != "E" and self._pos < len(self._s):
            parts.append(self._parse_unqualified_name())
        self._consume()  # 'E'
        result = "::".join(filter(None, parts))
        if qualifiers:
            result += f" {qualifiers}"
        self._substitutions.append(result)
        return result

    def _parse_local_name(self) -> str:
        self._consume()  # 'L'
        return f"(local) {self._parse_name()}"

    def _parse_unscoped_name(self) -> str:
        if self._peek() == "S":
            return self._parse_substitution()
        return self._parse_unqualified_name()

    def _parse_unqualified_name(self) -> str:
        """unqualified-name ::= <operator-name> | <ctor-dtor-name> | <source-name> | <template-param>"""
        c = self._peek()
        if c == "C":
            return self._parse_ctor()
        if c == "D":
            return self._parse_dtor()
        if c == "T":
            return self._parse_template_param()
        if c == "I":
            # template args applied to previous name
            return self._parse_template_args()
        if c.isdigit():
            return self._parse_source_name()
        if c in ("op", "c", "d") or c == "l" or c == "r" or c == "m":
            return self._parse_operator_name()
        return self._parse_source_name()

    def _parse_source_name(self) -> str:
        """source-name ::= <positive length number> <identifier>"""
        n = self._parse_number()
        name = self._consume(n)
        self._substitutions.append(name)
        return name

    def _parse_ctor(self) -> str:
        self._consume()  # 'C'
        code = self._consume()
        labels = {"1": "", "2": "(base)", "3": "(allocating)"}
        return labels.get(code, f"<ctor {code}>")

    def _parse_dtor(self) -> str:
        self._consume()  # 'D'
        code = self._consume()
        labels = {"0": "~(deleting)", "1": "~", "2": "~(base)"}
        return labels.get(code, f"<dtor {code}>")

    def _parse_template_param(self) -> str:
        self._consume()  # 'T'
        if self._peek() == "_":
            self._consume()
            return "T"
        n = self._parse_number()
        self._consume()  # '_'
        return f"T{n + 1}"

    def _parse_template_args(self) -> str:
        self._consume()  # 'I'
        args: List[str] = []
        while self._peek() != "E" and self._pos < len(self._s):
            args.append(self._parse_type())
        self._consume()  # 'E'
        return f"<{', '.join(args)}>"

    def _parse_operator_name(self) -> str:
        """Parse operators like 'ls' (<<), 'eq' (==), etc."""
        two = self._s[self._pos: self._pos + 2]
        _ops: Dict[str, str] = {
            "nw": "operator new",    "na": "operator new[]",
            "dl": "operator delete", "da": "operator delete[]",
            "ps": "operator+",       "ng": "operator-",
            "ad": "operator&",       "de": "operator*",
            "co": "operator~",       "pl": "operator+",
            "mi": "operator-",       "ml": "operator*",
            "dv": "operator/",       "rm": "operator%",
            "an": "operator&",       "or": "operator|",
            "eo": "operator^",       "aS": "operator=",
            "pL": "operator+=",      "mI": "operator-=",
            "mL": "operator*=",      "dV": "operator/=",
            "rM": "operator%=",      "aN": "operator&=",
            "oR": "operator|=",      "eO": "operator^=",
            "ls": "operator<<",      "rs": "operator>>",
            "lS": "operator<<=",     "rS": "operator>>=",
            "eq": "operator==",      "ne": "operator!=",
            "lt": "operator<",       "gt": "operator>",
            "le": "operator<=",      "ge": "operator>=",
            "nt": "operator!",       "aa": "operator&&",
            "oo": "operator||",      "pp": "operator++",
            "mm": "operator--",      "cm": "operator,",
            "pm": "operator->*",     "pt": "operator->",
            "cl": "operator()",      "ix": "operator[]",
            "qu": "operator?",
        }
        if two in _ops:
            self._consume(2)
            return _ops[two]
        self._consume()
        return f"operator<{two}>"

    def _parse_cv_qualifiers(self) -> str:
        qualifiers = []
        while self._peek() in ("K", "V", "r"):
            c = self._consume()
            if c == "K":
                qualifiers.append("const")
            elif c == "V":
                qualifiers.append("volatile")
            elif c == "r":
                qualifiers.append("restrict")
        return " ".join(qualifiers)

    def _parse_type(self) -> str:
        """Parse a type code."""
        c = self._peek()

        # Builtin types
        if c in _ITANIUM_BUILTINS:
            self._consume()
            return _ITANIUM_BUILTINS[c]

        two = self._s[self._pos: self._pos + 2]
        if two in _ITANIUM_BUILTINS:
            self._consume(2)
            return _ITANIUM_BUILTINS[two]

        if c == "P":
            self._consume()
            return f"{self._parse_type()}*"
        if c == "R":
            self._consume()
            return f"{self._parse_type()}&"
        if c == "O":
            self._consume()
            return f"{self._parse_type()}&&"
        if c == "K":
            self._consume()
            return f"const {self._parse_type()}"
        if c == "V":
            self._consume()
            return f"volatile {self._parse_type()}"
        if c == "A":
            return self._parse_array_type()
        if c == "F":
            return self._parse_function_type()
        if c == "N":
            return self._parse_nested_name()
        if c == "S":
            return self._parse_substitution()
        if c == "T":
            return self._parse_template_param()
        if c == "I":
            # Template args following a type
            base = self._substitutions[-1] if self._substitutions else ""
            args = self._parse_template_args()
            return f"{base}{args}"
        if c.isdigit():
            return self._parse_source_name()

        self._consume()
        return f"<type {c}>"

    def _parse_array_type(self) -> str:
        self._consume()  # 'A'
        dim = ""
        while self._peek() != "_":
            dim += self._consume()
        self._consume()  # '_'
        base = self._parse_type()
        return f"{base}[{dim}]"

    def _parse_function_type(self) -> str:
        self._consume()  # 'F'
        ret = self._parse_type()
        params: List[str] = []
        while self._peek() != "E" and self._pos < len(self._s):
            params.append(self._parse_type())
        self._consume()  # 'E'
        return f"{ret}({', '.join(params)})"

    def _parse_substitution(self) -> str:
        self._consume()  # 'S'
        c = self._peek()
        if c == "_":
            self._consume()
            return self._substitutions[-1] if self._substitutions else "<S_>"
        if c in _ITANIUM_SUBSTITUTIONS:
            self._consume()
            return _ITANIUM_SUBSTITUTIONS[c]
        # Sn_ where n is base-36 index
        idx_str = ""
        while self._peek() not in ("_", "") and not self._peek().startswith("E"):
            idx_str += self._consume()
        self._consume()  # '_'
        idx = int(idx_str, 36) + 1 if idx_str else 0
        if 0 <= idx < len(self._substitutions):
            return self._substitutions[idx]
        return f"<S{idx_str}_>"

    def _parse_number(self) -> int:
        start = self._pos
        while self._pos < len(self._s) and self._s[self._pos].isdigit():
            self._pos += 1
        return int(self._s[start: self._pos])

    def _parse_bare_function_type(self) -> List[str]:
        """Parse parameter type list."""
        params: List[str] = []
        while self._pos < len(self._s):
            params.append(self._parse_type())
        return params


class CppNameMangler:
    """
    C++ name mangler and demangler supporting Itanium and MSVC ABIs.
    """

    def __init__(self, abi: ManglingABI = ManglingABI.ITANIUM):
        self.abi = abi

    # ------------------------------------------------------------------
    # Demangling
    # ------------------------------------------------------------------

    def demangle(self, mangled: str) -> str:
        """
        Demangle a C++ symbol name.

        Tries the system c++filt/undname first, falls back to the built-in
        pure-Python parser when system tools are unavailable.

        Args:
            mangled: Mangled C++ symbol, e.g. '_ZN3foo3barEv'.

        Returns:
            Human-readable demangled name, e.g. 'foo::bar()'.
        """
        if self.abi == ManglingABI.ITANIUM:
            system_result = self._demangle_via_cxxfilt(mangled)
            if system_result:
                return system_result
            return ItaniumDemangler(mangled).demangle()
        else:
            return self._demangle_msvc(mangled)

    def _demangle_via_cxxfilt(self, mangled: str) -> Optional[str]:
        """Try to demangle using the system c++filt tool."""
        import subprocess
        try:
            result = subprocess.run(
                ["c++filt", mangled],
                capture_output=True,
                text=True,
                timeout=5,
            )
            demangled = result.stdout.strip()
            if demangled and demangled != mangled:
                return demangled
        except (FileNotFoundError, subprocess.TimeoutExpired, OSError):
            pass
        return None

    def _demangle_msvc(self, mangled: str) -> str:
        """Demangle an MSVC-mangled symbol (basic support)."""
        if not mangled.startswith("?"):
            return mangled
        # Attempt via DbgHelp on Windows, else return raw
        try:
            import ctypes
            dbghelp = ctypes.windll.DbgHelp  # type: ignore[attr-defined]
            buf = ctypes.create_string_buffer(256)
            ret = dbghelp.UnDecorateSymbolName(
                mangled.encode(), buf, ctypes.sizeof(buf), 0
            )
            if ret:
                return buf.value.decode()
        except (AttributeError, OSError):
            pass
        return mangled

    # ------------------------------------------------------------------
    # Mangling (Itanium) - for generating wrapper symbol names
    # ------------------------------------------------------------------

    def mangle_function(
        self, namespace: str, class_name: Optional[str], func_name: str
    ) -> str:
        """
        Generate an Itanium-mangled symbol for a simple function.

        This is sufficient for looking up pre-compiled C++ symbols to call
        from NexusLang wrappers.

        Args:
            namespace: Optional namespace (e.g. 'std').
            class_name: Optional class name (None for free functions).
            func_name: Function name.

        Returns:
            Mangled symbol name.
        """
        parts: List[str] = []
        if namespace:
            parts.append(f"{len(namespace)}{namespace}")
        if class_name:
            parts.append(f"{len(class_name)}{class_name}")
        parts.append(f"{len(func_name)}{func_name}")

        qualifier = "N" + "".join(parts) + "E" if len(parts) > 1 else parts[0]
        return f"_Z{qualifier}"

    def demangle_batch(self, symbols: List[str]) -> Dict[str, str]:
        """Demangle a list of symbols. Returns {mangled: demangled}."""
        return {sym: self.demangle(sym) for sym in symbols}


# ===========================================================================
# 2. C++ Class Wrapping
# ===========================================================================

@dataclass
class CppMethod:
    """Describes a C++ member function to expose via a C wrapper."""
    name: str
    param_types: List[str]    # C++ type names
    return_type: str          # C++ type name
    is_const: bool = False
    is_static: bool = False
    is_virtual: bool = False
    is_constructor: bool = False
    is_destructor: bool = False


@dataclass
class CppClassWrapper:
    """
    C++ class wrapper descriptor.

    Describes a C++ class to be wrapped in an 'extern "C"' C API so that
    NLPL's FFI (which only speaks C ABI) can create, call, and destroy
    C++ objects.

    The wrapper emits:
    - An opaque pointer typedef (nxl_<ClassName>_t)
    - A C constructor function (nxl_<ClassName>_new)
    - A C destructor function (nxl_<ClassName>_delete)
    - A C function for each wrapped method (nxl_<ClassName>_<methodName>)
    """
    class_name: str
    namespace: str = ""
    methods: List[CppMethod] = field(default_factory=list)
    constructor_params: List[str] = field(default_factory=list)
    has_default_constructor: bool = True
    include_header: str = ""   # e.g. "myclass.h"

    @property
    def qualified_name(self) -> str:
        if self.namespace:
            return f"{self.namespace}::{self.class_name}"
        return self.class_name

    @property
    def c_type_name(self) -> str:
        return f"nxl_{self.class_name.lower()}_t"

    @property
    def c_new_func(self) -> str:
        return f"nxl_{self.class_name.lower()}_new"

    @property
    def c_delete_func(self) -> str:
        return f"nxl_{self.class_name.lower()}_delete"

    def c_method_name(self, method_name: str) -> str:
        return f"nxl_{self.class_name.lower()}_{method_name.lower()}"


class CppWrapperGenerator:
    """
    Generates C wrapper code for C++ classes.

    Emits a C++ source file containing:
    - extern "C" wrapper functions
    - Proper exception catching (translates std::exception -> error codes)
    - Object lifecycle management (new/delete)

    The generated wrapper is compiled by the project's C++ compiler and
    linked with NLPL-compiled output.
    """

    def __init__(self, exception_handling: bool = True):
        """
        Args:
            exception_handling: Whether to catch exceptions in wrappers
                                 and return error codes instead of propagating.
        """
        self.exception_handling = exception_handling

    def generate_header(self, wrapper: CppClassWrapper) -> str:
        """
        Generate the C header file for the wrapper.

        Returns:
            Contents of the .h file as a string.
        """
        guard = f"NLPL_{wrapper.class_name.upper()}_WRAPPER_H"
        lines: List[str] = [
            f"/* Auto-generated by NexusLang C++ wrapper generator */",
            f"/* DO NOT EDIT - regenerate with nlpl-bindgen */",
            f"",
            f"#ifndef {guard}",
            f"#define {guard}",
            f"",
            f"#include <stddef.h>",
            f"#include <stdint.h>",
            f"",
            f"#ifdef __cplusplus",
            f'extern "C" {{',
            f"#endif",
            f"",
            f"/* Opaque handle for {wrapper.qualified_name} */",
            f"typedef struct {wrapper.c_type_name}_s* {wrapper.c_type_name};",
            f"",
            f"/* Error codes */",
            f"#define NLPL_FFI_OK            0",
            f"#define NLPL_FFI_ERR_EXCEPTION (-1)",
            f"#define NLPL_FFI_ERR_NULL      (-2)",
            f"",
            f"/* Constructor */",
        ]

        # Constructor
        if wrapper.has_default_constructor:
            params = ", ".join(f"/* {t} */ void* p{i}" for i, t in enumerate(wrapper.constructor_params))
            lines.append(f"{wrapper.c_type_name} {wrapper.c_new_func}({params});")

        # Destructor
        lines.append(f"void {wrapper.c_delete_func}({wrapper.c_type_name} self);")
        lines.append("")

        # Methods
        lines.append("/* Methods */")
        for method in wrapper.methods:
            if method.is_constructor or method.is_destructor:
                continue
            c_params = self._build_c_params(wrapper, method)
            c_return = self._cpp_type_to_c(method.return_type)
            lines.append(
                f"int {wrapper.c_method_name(method.name)}"
                f"({', '.join(c_params)}, {c_return}* out_result);"
            )

        lines += [
            "",
            "#ifdef __cplusplus",
            "}",
            "#endif",
            "",
            f"#endif /* {guard} */",
        ]
        return "\n".join(lines)

    def generate_implementation(self, wrapper: CppClassWrapper) -> str:
        """
        Generate the C++ implementation file for the wrapper.

        Returns:
            Contents of the .cpp file as a string.
        """
        lines: List[str] = [
            f"/* Auto-generated by NexusLang C++ wrapper generator */",
            f"/* DO NOT EDIT - regenerate with nlpl-bindgen */",
            f"",
        ]
        if wrapper.include_header:
            lines.append(f'#include "{wrapper.include_header}"')
        lines += [
            f'#include "{wrapper.class_name.lower()}_wrapper.h"',
            f"#include <stdexcept>",
            f"#include <new>",
            f"",
            f'extern "C" {{',
            f"",
            f"/* === Constructor === */",
        ]

        if wrapper.has_default_constructor:
            param_decls = ", ".join(
                f"void* p{i}" for i, _ in enumerate(wrapper.constructor_params)
            )
            param_casts = ", ".join(
                f"*static_cast<{t}*>(p{i})"
                for i, t in enumerate(wrapper.constructor_params)
            )
            lines.append(f"{wrapper.c_type_name} {wrapper.c_new_func}({param_decls}) {{")
            if self.exception_handling:
                lines += [
                    f"    try {{",
                    f"        return reinterpret_cast<{wrapper.c_type_name}>(",
                    f"            new {wrapper.qualified_name}({param_casts}));",
                    f"    }} catch (...) {{ return nullptr; }}",
                    f"}}",
                ]
            else:
                lines += [
                    f"    return reinterpret_cast<{wrapper.c_type_name}>(",
                    f"        new {wrapper.qualified_name}({param_casts}));",
                    f"}}",
                ]

        # Destructor
        lines += [
            f"",
            f"/* === Destructor === */",
            f"void {wrapper.c_delete_func}({wrapper.c_type_name} self) {{",
            f"    if (!self) return;",
            f"    delete reinterpret_cast<{wrapper.qualified_name}*>(self);",
            f"}}",
            f"",
            f"/* === Methods === */",
        ]

        for method in wrapper.methods:
            if method.is_constructor or method.is_destructor:
                continue
            lines += self._generate_method_wrapper(wrapper, method)
            lines.append("")

        lines.append('} /* extern "C" */')
        return "\n".join(lines)

    def _build_c_params(
        self, wrapper: CppClassWrapper, method: CppMethod
    ) -> List[str]:
        """Build C parameter list for a method wrapper."""
        params: List[str] = []
        if not method.is_static:
            qualifier = "const " if method.is_const else ""
            params.append(f"{qualifier}{wrapper.c_type_name} self")
        for i, ptype in enumerate(method.param_types):
            params.append(f"{self._cpp_type_to_c(ptype)} p{i}")
        return params

    def _cpp_type_to_c(self, cpp_type: str) -> str:
        """Convert a C++ type name to its C-ABI-compatible equivalent."""
        _map = {
            "std::string": "const char*",
            "string":      "const char*",
            "bool":        "int",
            "size_t":      "size_t",
            "void":        "void",
            "int":         "int",
            "double":      "double",
            "float":       "float",
            "long":        "long",
            "char":        "char",
        }
        return _map.get(cpp_type, cpp_type)

    def _generate_method_wrapper(
        self, wrapper: CppClassWrapper, method: CppMethod
    ) -> List[str]:
        """Generate wrapper lines for a single method."""
        c_params = self._build_c_params(wrapper, method)
        c_return = self._cpp_type_to_c(method.return_type)
        has_return = method.return_type not in ("void", "Void")

        func_sig = (
            f"int {wrapper.c_method_name(method.name)}"
            f"({', '.join(c_params)}, {c_return}* out_result)"
        )

        cast_args = ", ".join(f"p{i}" for i in range(len(method.param_types)))
        if method.is_static:
            call_expr = f"{wrapper.qualified_name}::{method.name}({cast_args})"
        else:
            obj_ptr = f"reinterpret_cast<{wrapper.qualified_name}*>(const_cast<void*>(reinterpret_cast<const void*>(self)))"
            call_expr = f"{obj_ptr}->{method.name}({cast_args})"

        lines = [f"{func_sig} {{"]
        if not method.is_static:
            lines += [
                f"    if (!self) return NLPL_FFI_ERR_NULL;",
            ]
        if self.exception_handling:
            lines.append(f"    try {{")
            if has_return:
                lines += [
                    f"        if (out_result) *out_result = {call_expr};",
                    f"        else {{ (void)({call_expr}); }}",
                ]
            else:
                lines.append(f"        {call_expr};")
            lines += [
                f"        return NLPL_FFI_OK;",
                f"    }} catch (const std::exception&) {{",
                f"        return NLPL_FFI_ERR_EXCEPTION;",
                f"    }} catch (...) {{",
                f"        return NLPL_FFI_ERR_EXCEPTION;",
                f"    }}",
            ]
        else:
            if has_return:
                lines += [
                    f"    if (out_result) *out_result = {call_expr};",
                    f"    else {{ (void)({call_expr}); }}",
                ]
            else:
                lines.append(f"    {call_expr};")
            lines.append(f"    return NLPL_FFI_OK;")
        lines.append(f"}}")
        return lines


# ===========================================================================
# 3. Template Instantiation
# ===========================================================================

@dataclass
class TemplateInstance:
    """
    Describes a pre-instantiated C++ template function or class to expose
    as an ordinary symbol for NLPL's FFI.

    Standard C++ templates are instantiated at compile time; the resulting
    symbols are regular mangled names that can be declared as 'extern "C"'
    wrapped functions or called directly via FFI if their mangled name is known.
    """
    template_name: str           # e.g. 'std::vector', 'sort'
    type_params: List[str]       # e.g. ['int', 'std::allocator<int>']
    namespace: str = ""
    kind: str = "function"       # 'function' or 'class'
    wrapper_func_name: str = ""  # Override output C wrapper name


class TemplateInstantiationHelper:
    """
    Helper for working with C++ template instantiations in NexusLang FFI.

    Primary use case: register a known pre-instantiated template symbol
    under an NLPL-visible alias, so it can be called via the normal
    extern-function FFI mechanism.
    """

    def __init__(self, mangler: Optional[CppNameMangler] = None):
        self.mangler = mangler or CppNameMangler(ManglingABI.ITANIUM)
        self._instances: Dict[str, TemplateInstance] = {}

    def register(self, instance: TemplateInstance) -> str:
        """
        Register a template instance.

        Returns:
            The recommended C wrapper function name.
        """
        alias = instance.wrapper_func_name or self._make_alias(instance)
        self._instances[alias] = instance
        return alias

    def _make_alias(self, inst: TemplateInstance) -> str:
        """Generate a C-safe alias name for a template instance."""
        parts = []
        if inst.namespace:
            parts.append(inst.namespace.replace("::", "_"))
        parts.append(inst.template_name.replace("::", "_"))
        for tp in inst.type_params:
            safe = re.sub(r"[^a-zA-Z0-9_]", "_", tp)
            parts.append(safe)
        return "nxl_tmpl_" + "_".join(parts)

    def generate_instantiation_header(self, inst: TemplateInstance) -> str:
        """
        Generate an 'extern "C"' C++ header declaring a wrapper that exposes
        a specific template instantiation as a plain C function.

        Args:
            inst: Template instance descriptor.

        Returns:
            C++ header content as string.
        """
        alias = inst.wrapper_func_name or self._make_alias(inst)
        type_args = ", ".join(inst.type_params)
        qualified = f"{inst.namespace}::{inst.template_name}" if inst.namespace else inst.template_name
        tmpl_type = f"{qualified}<{type_args}>"

        lines = [
            f"/* NexusLang template instantiation wrapper: {tmpl_type} */",
            f'#ifdef __cplusplus',
            f'extern "C" {{',
            f'#endif',
            f"",
            f"/* Forward declare the template instantiation alias */",
            f"/* Include the appropriate C++ header before this file */",
            f"void {alias}_init(void* storage, size_t size);",
            f"void {alias}_destroy(void* storage);",
            f"",
            f"#ifdef __cplusplus",
            f"}}",
            f"#endif",
        ]
        return "\n".join(lines)

    def generate_instantiation_impl(self, inst: TemplateInstance) -> str:
        """
        Generate the .cpp file that forces the compiler to emit a specific
        template instantiation and exposes it via extern "C".

        Args:
            inst: Template instance descriptor.

        Returns:
            C++ source file content as string.
        """
        alias = inst.wrapper_func_name or self._make_alias(inst)
        type_args = ", ".join(inst.type_params)
        qualified = f"{inst.namespace}::{inst.template_name}" if inst.namespace else inst.template_name
        tmpl_type = f"{qualified}<{type_args}>"

        lines = [
            f"/* NexusLang template instantiation implementation: {tmpl_type} */",
            f"/* Auto-generated by NexusLang template instantiation helper */",
            f"",
            f"#include <cstddef>",
            f"#include <new>",
            f"/* Include the real template header here: */",
            f"/* #include <{inst.template_name.lower()}>  // e.g. <vector>, <map> */",
            f"",
            f"/* Explicit instantiation to force symbol emission */",
            f"template class {tmpl_type};",
            f"",
            f'extern "C" {{',
            f"",
            f"void {alias}_init(void* storage, size_t size) {{",
            f"    if (size < sizeof({tmpl_type})) return;",
            f"    new (storage) {tmpl_type}();",
            f"}}",
            f"",
            f"void {alias}_destroy(void* storage) {{",
            f"    reinterpret_cast<{tmpl_type}*>(storage)->~{inst.template_name.split('<')[0]}();",
            f"}}",
            f"",
            f'}} /* extern "C" */',
        ]
        return "\n".join(lines)

    def list_instances(self) -> List[TemplateInstance]:
        return list(self._instances.values())


# ===========================================================================
# 4. Exception Handling Across FFI Boundary
# ===========================================================================

class CppExceptionClass(Enum):
    """Known C++ exception hierarchy classes."""
    STD_EXCEPTION       = "std::exception"
    STD_RUNTIME_ERROR   = "std::runtime_error"
    STD_LOGIC_ERROR     = "std::logic_error"
    STD_BAD_ALLOC       = "std::bad_alloc"
    STD_RANGE_ERROR     = "std::range_error"
    STD_OUT_OF_RANGE    = "std::out_of_range"
    STD_OVERFLOW_ERROR  = "std::overflow_error"
    STD_UNDERFLOW_ERROR = "std::underflow_error"
    STD_INVALID_ARG     = "std::invalid_argument"
    STD_DOMAIN_ERROR    = "std::domain_error"
    STD_LENGTH_ERROR    = "std::length_error"
    UNKNOWN             = "..."


@dataclass
class FFIExceptionRecord:
    """A C++ exception that was caught at the FFI boundary."""
    exception_class: CppExceptionClass
    message: str
    source_function: str
    was_rethrown: bool = False


class CppExceptionBridge:
    """
    Generates C++ wrapper code that catches exceptions at the FFI boundary
    and translates them into C-compatible error codes + message strings.

    The pattern used is the 'error_code + out-param' style, compatible with
    all C ABIs.

    The two main patterns generated:

    Pattern A - Error code return style:
        int wrapped_func(..., char* err_buf, int err_buf_size);

    Pattern B - Thread-local last-error style (like GetLastError()):
        void nxl_ffi_clear_exception(void);
        const char* nxl_ffi_get_exception(void);
        int nxl_ffi_exception_code(void);

    Both patterns are generated; callers can choose.
    """

    _EXCEPTION_CODES: Dict[CppExceptionClass, int] = {
        CppExceptionClass.STD_EXCEPTION:       1,
        CppExceptionClass.STD_RUNTIME_ERROR:   2,
        CppExceptionClass.STD_LOGIC_ERROR:     3,
        CppExceptionClass.STD_BAD_ALLOC:       4,
        CppExceptionClass.STD_RANGE_ERROR:     5,
        CppExceptionClass.STD_OUT_OF_RANGE:    6,
        CppExceptionClass.STD_OVERFLOW_ERROR:  7,
        CppExceptionClass.STD_UNDERFLOW_ERROR: 8,
        CppExceptionClass.STD_INVALID_ARG:     9,
        CppExceptionClass.STD_DOMAIN_ERROR:    10,
        CppExceptionClass.STD_LENGTH_ERROR:    11,
        CppExceptionClass.UNKNOWN:             99,
    }

    def generate_thread_local_exception_infrastructure(self) -> str:
        """
        Generate a C++ source snippet that provides:
        - Thread-local storage for the last exception
        - nxl_ffi_clear_exception()
        - nxl_ffi_get_exception()
        - nxl_ffi_exception_code()
        - nxl_ffi_set_exception() (internal)
        - NLPL_TRY / NLPL_CATCH_ALL macro for wrapping calls

        Returns:
            C++ source snippet as string.
        """
        return textwrap.dedent("""\
            /* NexusLang FFI Exception Bridge - thread-local last-error pattern */
            /* Include this once in your C++ wrapper compilation unit       */

            #include <exception>
            #include <stdexcept>
            #include <cstring>
            #include <cstddef>

            #ifdef __cplusplus

            namespace {
                thread_local int nxl_last_exc_code = 0;
                thread_local char nxl_last_exc_msg[1024] = {0};

                void nxl_ffi_set_exception(int code, const char* msg) {
                    nxl_last_exc_code = code;
                    if (msg) {
                        std::strncpy(nxl_last_exc_msg, msg, sizeof(nxl_last_exc_msg) - 1);
                        nxl_last_exc_msg[sizeof(nxl_last_exc_msg) - 1] = '\\0';
                    } else {
                        nxl_last_exc_msg[0] = '\\0';
                    }
                }
            }

            extern "C" {

            void nxl_ffi_clear_exception(void) {
                nxl_last_exc_code = 0;
                nxl_last_exc_msg[0] = '\\0';
            }

            const char* nxl_ffi_get_exception(void) {
                return nxl_last_exc_msg;
            }

            int nxl_ffi_exception_code(void) {
                return nxl_last_exc_code;
            }

            } /* extern "C" */

            /* Macro: wrap a C++ call, store any exception, return error code */
            #define NLPL_TRY_CALL(expr, ok_retval, err_retval)       \\
                do {                                                   \\
                    nxl_ffi_clear_exception();                        \\
                    try { (void)(expr); return (ok_retval); }         \\
                    catch (const std::bad_alloc& e)      { nxl_ffi_set_exception(4, e.what()); } \\
                    catch (const std::out_of_range& e)   { nxl_ffi_set_exception(6, e.what()); } \\
                    catch (const std::invalid_argument& e){ nxl_ffi_set_exception(9, e.what()); } \\
                    catch (const std::runtime_error& e)  { nxl_ffi_set_exception(2, e.what()); } \\
                    catch (const std::logic_error& e)    { nxl_ffi_set_exception(3, e.what()); } \\
                    catch (const std::exception& e)      { nxl_ffi_set_exception(1, e.what()); } \\
                    catch (...)                          { nxl_ffi_set_exception(99, "unknown exception"); } \\
                    return (err_retval);                               \\
                } while (0)

            #define NLPL_TRY_CALL_VOID(expr)                          \\
                do {                                                   \\
                    nxl_ffi_clear_exception();                        \\
                    try { (void)(expr); }                              \\
                    catch (const std::bad_alloc& e)      { nxl_ffi_set_exception(4, e.what()); } \\
                    catch (const std::out_of_range& e)   { nxl_ffi_set_exception(6, e.what()); } \\
                    catch (const std::invalid_argument& e){ nxl_ffi_set_exception(9, e.what()); } \\
                    catch (const std::runtime_error& e)  { nxl_ffi_set_exception(2, e.what()); } \\
                    catch (const std::logic_error& e)    { nxl_ffi_set_exception(3, e.what()); } \\
                    catch (const std::exception& e)      { nxl_ffi_set_exception(1, e.what()); } \\
                    catch (...)                          { nxl_ffi_set_exception(99, "unknown exception"); } \\
                } while (0)

            #endif /* __cplusplus */
        """)

    def exception_code_for(self, exc_class: CppExceptionClass) -> int:
        """Return the numeric code for a known exception class."""
        return self._EXCEPTION_CODES.get(exc_class, 99)

    def exception_class_for_code(self, code: int) -> CppExceptionClass:
        """Reverse-lookup exception class from numeric code."""
        for exc, c in self._EXCEPTION_CODES.items():
            if c == code:
                return exc
        return CppExceptionClass.UNKNOWN


# ===========================================================================
# 5. RTTI Support
# ===========================================================================

@dataclass
class RTTITypeInfo:
    """
    Represents C++ RTTI type_info for a registered class.
    NexusLang can query this to implement dynamic dispatch and type queries
    across the FFI boundary.
    """
    type_name: str                       # Demangled type name
    mangled_name: str                    # Mangled symbol (Itanium: _ZTI...)
    is_polymorphic: bool = True          # Has at least one virtual function
    base_classes: List[str] = field(default_factory=list)  # Direct base type names


class RTTISupport:
    """
    RTTI (Run-Time Type Information) support for C++ interop.

    Provides:
    - Type registration for known C++ classes
    - dynamic_cast equivalent via C wrapper (returns null on failure)
    - typeid equivalent (returns type name string)
    - Type hierarchy queries

    C++ RTTI cannot cross the C ABI directly; this generates C wrappers
    that perform the actual C++ casts and return opaque handles.
    """

    def __init__(self, mangler: Optional[CppNameMangler] = None):
        self.mangler = mangler or CppNameMangler(ManglingABI.ITANIUM)
        self._types: Dict[str, RTTITypeInfo] = {}

    def register_type(self, info: RTTITypeInfo) -> None:
        """Register a C++ class for RTTI operations."""
        self._types[info.type_name] = info

    def get_type_info(self, type_name: str) -> Optional[RTTITypeInfo]:
        return self._types.get(type_name)

    def is_subclass(self, derived: str, base: str) -> bool:
        """Check if derived is a subclass of base (using registered hierarchy)."""
        info = self._types.get(derived)
        if info is None:
            return False
        if base in info.base_classes:
            return True
        # Transitive check
        return any(self.is_subclass(b, base) for b in info.base_classes)

    def generate_rtti_wrappers(self, base_type: str, derived_types: List[str]) -> str:
        """
        Generate C++ wrapper code that exposes dynamic_cast and typeid
        as C functions.

        Args:
            base_type: The C++ base class (e.g. 'Animal').
            derived_types: List of derived types to support casting to.

        Returns:
            C++ source code for the RTTI wrappers.
        """
        lines = [
            f"/* NexusLang RTTI wrappers for {base_type} hierarchy */",
            f"/* Auto-generated by NexusLang RTTI support module   */",
            f"",
            f'#ifdef __cplusplus',
            f'extern "C" {{',
            f'#endif',
            f"",
            f"/* typeid: return demangled type name of the pointed-to object */",
            f"const char* nxl_rtti_typeid_{base_type.lower()}(const {base_type}* obj) {{",
            f"    if (!obj) return \"(null)\";",
            f"    return typeid(*obj).name();   /* Itanium: mangled; use __cxa_demangle to demangle */",
            f"}}",
            f"",
        ]

        for derived in derived_types:
            lines += [
                f"/* dynamic_cast {base_type}* -> {derived}* */",
                f"{derived}* nxl_rtti_cast_{base_type.lower()}_to_{derived.lower()}({base_type}* obj) {{",
                f"    return dynamic_cast<{derived}*>(obj);",
                f"}}",
                f"",
                f"/* is-a check: returns 1 if obj is a {derived}, 0 otherwise */",
                f"int nxl_rtti_isa_{derived.lower()}(const {base_type}* obj) {{",
                f"    return dynamic_cast<const {derived}*>(obj) != nullptr ? 1 : 0;",
                f"}}",
                f"",
            ]

        lines += [
            f"#ifdef __cplusplus",
            f"}}",
            f"#endif",
        ]
        return "\n".join(lines)

    def generate_rtti_header(self, base_type: str, derived_types: List[str]) -> str:
        """Generate the corresponding C header for the RTTI wrappers."""
        guard = f"NLPL_RTTI_{base_type.upper()}_H"
        lines = [
            f"#ifndef {guard}",
            f"#define {guard}",
            f"",
            f'#ifdef __cplusplus',
            f'extern "C" {{',
            f'#endif',
            f"",
            f"const char* nxl_rtti_typeid_{base_type.lower()}(const void* obj);",
        ]
        for derived in derived_types:
            lines += [
                f"void* nxl_rtti_cast_{base_type.lower()}_to_{derived.lower()}(void* obj);",
                f"int   nxl_rtti_isa_{derived.lower()}(const void* obj);",
            ]
        lines += [
            f"",
            f"#ifdef __cplusplus",
            f"}}",
            f"#endif",
            f"",
            f"#endif /* {guard} */",
        ]
        return "\n".join(lines)


# ===========================================================================
# High-level C++ interop facade
# ===========================================================================

class CppInterop:
    """
    Unified C++ interoperability facade for NexusLang.

    Combines name mangling, class wrapping, template instantiation,
    exception handling, and RTTI into one discoverable API.

    Usage::

        cpp = CppInterop()

        # Demangle a symbol from nm/objdump output
        name = cpp.demangle("_ZN3foo3barEv")       # -> "foo::bar()"

        # Describe and generate a class wrapper (e.g. for std::string)
        wrapper = CppClassWrapper("basic_string", namespace="std",
                                  methods=[CppMethod("size", [], "size_t")])
        header_src = cpp.generate_class_header(wrapper)
        impl_src   = cpp.generate_class_impl(wrapper)

        # Register a template instance
        inst = TemplateInstance("vector", ["int"], namespace="std", kind="class")
        alias = cpp.register_template(inst)

        # Exception error codes
        err_code = cpp.exception_code(CppExceptionClass.STD_BAD_ALLOC)

        # RTTI wrappers
        rtti_src = cpp.generate_rtti_wrappers("Animal", ["Dog", "Cat"])
    """

    def __init__(self, abi: ManglingABI = ManglingABI.ITANIUM):
        self.abi = abi
        self.mangler = CppNameMangler(abi)
        self._class_gen = CppWrapperGenerator(exception_handling=True)
        self._template_helper = TemplateInstantiationHelper(self.mangler)
        self._exception_bridge = CppExceptionBridge()
        self._rtti = RTTISupport(self.mangler)

    # ------------------------------------------------------------------
    # Name mangling
    # ------------------------------------------------------------------

    def demangle(self, mangled: str) -> str:
        return self.mangler.demangle(mangled)

    def demangle_batch(self, symbols: List[str]) -> Dict[str, str]:
        return self.mangler.demangle_batch(symbols)

    def mangle_function(
        self, namespace: str, class_name: Optional[str], func_name: str
    ) -> str:
        return self.mangler.mangle_function(namespace, class_name, func_name)

    # ------------------------------------------------------------------
    # Class wrapping
    # ------------------------------------------------------------------

    def generate_class_header(self, wrapper: CppClassWrapper) -> str:
        return self._class_gen.generate_header(wrapper)

    def generate_class_impl(self, wrapper: CppClassWrapper) -> str:
        return self._class_gen.generate_implementation(wrapper)

    # ------------------------------------------------------------------
    # Template instantiation
    # ------------------------------------------------------------------

    def register_template(self, instance: TemplateInstance) -> str:
        return self._template_helper.register(instance)

    def generate_template_header(self, instance: TemplateInstance) -> str:
        return self._template_helper.generate_instantiation_header(instance)

    def generate_template_impl(self, instance: TemplateInstance) -> str:
        return self._template_helper.generate_instantiation_impl(instance)

    # ------------------------------------------------------------------
    # Exception handling
    # ------------------------------------------------------------------

    def generate_exception_infrastructure(self) -> str:
        return self._exception_bridge.generate_thread_local_exception_infrastructure()

    def exception_code(self, exc_class: CppExceptionClass) -> int:
        return self._exception_bridge.exception_code_for(exc_class)

    def exception_class_for_code(self, code: int) -> CppExceptionClass:
        return self._exception_bridge.exception_class_for_code(code)

    # ------------------------------------------------------------------
    # RTTI
    # ------------------------------------------------------------------

    def register_rtti_type(self, info: RTTITypeInfo) -> None:
        self._rtti.register_type(info)

    def generate_rtti_wrappers(
        self, base_type: str, derived_types: List[str]
    ) -> str:
        return self._rtti.generate_rtti_wrappers(base_type, derived_types)

    def generate_rtti_header(
        self, base_type: str, derived_types: List[str]
    ) -> str:
        return self._rtti.generate_rtti_header(base_type, derived_types)

    def is_subclass(self, derived: str, base: str) -> bool:
        return self._rtti.is_subclass(derived, base)
