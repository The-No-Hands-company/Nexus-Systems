"""
JIT Code Generator
==================

Transforms NexusLang function AST nodes into compiled Python callables.  This is
the core "runtime code generation" component of the NexusLang JIT pipeline.

For each NexusLang function definition the generator:

1. Walks the AST and emits equivalent Python source code.
2. Compiles the source with Python's built-in compile().
3. Wraps it in a real Python function object (not an AST-walker).

The generated callable is significantly faster than the interpreter's
AST-walking execute() loop because Python's bytecode evaluator is much
more efficient than repeated dynamic attribute access and isinstance() tests
during tree traversal.

Tier differentiation
--------------------

opt_level=1 (Baseline JIT)
    Straightforward AST-to-Python translation.  Fast to generate; no
    speculation.

opt_level=2 (Optimizing JIT)
    Inserts type-guard checks at function entry based on type_hints gathered
    by TypeFeedbackCollector.  A JITGuardFailed exception triggers
    deoptimization back to the interpreter tier.  When the types remain
    stable (common case) the guard overhead is a single isinstance() per
    typed parameter and is essentially free compared to the body savings.
"""
from __future__ import annotations

from typing import Any, Callable, Dict, List, Optional, Type

__all__ = [
    "NLPLCodeGenerator",
    "JITGuardFailed",
    "CodeGenError",
    "_make_call_helper",
    "_make_print_helper",
]


# ---------------------------------------------------------------------------
# Exceptions
# ---------------------------------------------------------------------------

class JITGuardFailed(Exception):
    """
    Raised by JIT-compiled code when a type-guard check fails at runtime.

    The TieredCompiler catches this and deoptimizes the function back to
    the interpreter tier so execution can continue correctly.
    """


class CodeGenError(Exception):
    """Raised when code generation is unable to handle an AST node."""


# ---------------------------------------------------------------------------
# Operator mapping — NexusLang natural-language operators -> Python operators
# ---------------------------------------------------------------------------

_OP_MAP: Dict[str, str] = {
    # Arithmetic
    "+": "+", "plus": "+", "added to": "+",
    "-": "-", "minus": "-", "subtracted from": "-",
    "*": "*", "times": "*", "multiplied by": "*",
    "/": "/", "divided by": "/",
    "//": "//", "integer divided by": "//",
    "%": "%", "modulo": "%", "mod": "%",
    "**": "**", "pow": "**", "power": "**", "to the power of": "**",
    # Comparison
    "==": "==", "is equal to": "==", "equals": "==",
    "!=": "!=", "is not equal to": "!=",
    "<": "<", "is less than": "<",
    "<=": "<=", "is less than or equal to": "<=",
    ">": ">", "is greater than": ">",
    ">=": ">=", "is greater than or equal to": ">=",
    # Logical
    "and": "and", "or": "or",
    # Bitwise
    "&": "&", "|": "|", "^": "^", "<<": "<<", ">>": ">>",
}

# NexusLang type names -> Python isinstance() builtin type names
_TYPE_GUARD_MAP: Dict[str, str] = {
    "Integer": "int",
    "Float": "float",
    "String": "str",
    "Boolean": "bool",
    "List": "list",
    "Dictionary": "dict",
}


# ---------------------------------------------------------------------------
# Code generator
# ---------------------------------------------------------------------------

class NLPLCodeGenerator:
    """
    Generates Python source code from an NexusLang function definition AST and
    compiles it into a Python callable.

    Usage::

        gen = NLPLCodeGenerator()

        # Baseline compilation (no guards)
        fn = gen.compile_function(func_def, interpreter, opt_level=1)

        # Optimizing compilation with type guards
        fn = gen.compile_function(func_def, interpreter,
                                  type_hints={"param_0": "Integer",
                                              "param_1": "Integer"},
                                  opt_level=2)
        result = fn(3, 4)
    """

    # ------------------------------------------------------------------
    # Public interface
    # ------------------------------------------------------------------

    def generate_function(
        self,
        func_def: Any,
        type_hints: Optional[Dict[str, str]] = None,
        opt_level: int = 1,
    ) -> str:
        """
        Generate Python source code for a function definition.

        Args:
            func_def:   NexusLang FunctionDefinition AST node.  Must expose
                        ``.name``, ``.parameters`` (list), and ``.body`` (list).
            type_hints: Mapping ``"param_N"`` -> NexusLang type name, produced by
                        TypeFeedbackCollector.  Used only at opt_level >= 2.
            opt_level:  Compiler optimization level.  1 = baseline (no
                        speculation), 2+ = insert type-guard checks.

        Returns:
            Python source string that defines the function
            ``_jit_<funcname>(**params)``.
        """
        type_hints = type_hints or {}
        func_name = getattr(func_def, "name", "unknown")
        params = list(getattr(func_def, "parameters", None) or [])
        body = list(getattr(func_def, "body", None) or [])

        param_names = [
            getattr(p, "name", f"_p{i}") for i, p in enumerate(params)
        ]
        sig = ", ".join(param_names)

        lines: List[str] = [f"def _jit_{func_name}({sig}):"]
        inner: List[str] = []

        # Tier-2 type guards at function entry
        if opt_level >= 2:
            for idx, pname in enumerate(param_names):
                hint = type_hints.get(f"param_{idx}")
                py_type = _TYPE_GUARD_MAP.get(hint or "")
                if py_type:
                    inner.append(
                        f"if not isinstance({pname}, {py_type}): "
                        f"raise _GuardFailed('guard: {pname} expected {hint}')"
                    )

        # Translate body statements
        for stmt in body:
            inner.extend(self._gen_stmt(stmt, depth=0))

        if not inner:
            inner.append("pass")

        lines.extend("    " + ln for ln in inner)
        return "\n".join(lines) + "\n"

    def compile_function(
        self,
        func_def: Any,
        interpreter: Any,
        type_hints: Optional[Dict[str, str]] = None,
        opt_level: int = 1,
    ) -> Callable:
        """
        Generate, compile, and return an executable Python callable for an
        NexusLang function.

        The returned callable:
        - Takes the same positional arguments as the NexusLang function.
        - Dispatches NexusLang function calls back through the interpreter so
          that non-compiled callees still work correctly.
        - At opt_level >= 2, raises JITGuardFailed on type mismatches so
          the TieredCompiler can deoptimize.

        Args:
            func_def:    NexusLang FunctionDefinition AST node.
            interpreter: The runtime interpreter instance.
            type_hints:  Type-feedback hints (from TypeFeedbackCollector).
            opt_level:   1 = baseline, 2 = optimizing with guards.

        Returns:
            Python callable.

        Raises:
            CodeGenError: If the generated source contains a syntax error or
                          the compilation otherwise fails.
        """
        source = self.generate_function(func_def, type_hints, opt_level)
        func_name = getattr(func_def, "name", "unknown")

        ns: Dict[str, Any] = {
            "_GuardFailed": JITGuardFailed,
            "_call": _make_call_helper(interpreter),
            "_print": _make_print_helper(interpreter),
        }

        # Security note: `source` here is machine-generated Python from the NexusLang
        # AST node — not user-supplied strings. The exec namespace `ns` is a
        # tightly-controlled dict containing only the JIT helper functions.
        # This is intentional and audited. See docs/contributing/architecture.md.
        try:
            code = compile(source, f"<jit:{func_name}>", "exec")
        except SyntaxError as exc:
            raise CodeGenError(
                f"Syntax error in generated code for '{func_name}': {exc}\n"
                f"--- generated source ---\n{source}"
            ) from exc

        try:
            exec(code, ns)  # noqa: S102 – controlled namespace
        except Exception as exc:
            raise CodeGenError(
                f"exec() failed for '{func_name}': {exc}\n"
                f"--- generated source ---\n{source}"
            ) from exc

        jit_name = f"_jit_{func_name}"
        if jit_name not in ns:
            raise CodeGenError(
                f"Generated function '{jit_name}' not found after exec().\n"
                f"--- generated source ---\n{source}"
            )
        return ns[jit_name]

    # ------------------------------------------------------------------
    # Statement translation
    # ------------------------------------------------------------------

    def _gen_stmt(self, node: Any, depth: int = 0) -> List[str]:
        """Translate a single statement node into a list of source lines."""
        if node is None:
            return ["pass"]
        kind = type(node).__name__
        method = getattr(self, f"_stmt_{kind}", None)
        if method is not None:
            return method(node, depth)
        # Try PrintStatement aliases
        if kind in ("PrintStatement", "Print", "PrintTextStatement"):
            return self._stmt_print_alias(node, depth)
        if kind == "FunctionCall":
            return [self._gen_expr(node)]
        if kind in ("Comment", "PassStatement"):
            return ["pass"]
        # Unknown node — emit a no-op comment so the function still compiles
        return [f"pass  # unhandled statement: {kind}"]

    def _stmt_VariableDeclaration(self, node: Any, depth: int) -> List[str]:
        name = getattr(node, "name", "_unknown")
        value = getattr(node, "value", None)
        return [f"{name} = {self._gen_expr(value)}"]

    def _stmt_AssignmentStatement(self, node: Any, depth: int) -> List[str]:
        target = getattr(node, "target", None) or getattr(node, "name", "_unknown")
        value = getattr(node, "value", None)
        if not isinstance(target, str):
            target = self._gen_expr(target)
        return [f"{target} = {self._gen_expr(value)}"]

    def _stmt_IndexAssignment(self, node: Any, depth: int) -> List[str]:
        # IndexAssignment.target is an IndexExpression(array_expr, index_expr)
        target_node = getattr(node, "target", None)
        value = self._gen_expr(getattr(node, "value", None))
        if target_node is not None and type(target_node).__name__ == "IndexExpression":
            arr = self._gen_expr(getattr(target_node, "array_expr", None))
            idx = self._gen_expr(getattr(target_node, "index_expr", None))
            return [f"{arr}[{idx}] = {value}"]
        # Fallback for nodes with direct target/index attributes
        target = self._gen_expr(target_node)
        index = self._gen_expr(getattr(node, "index", None))
        return [f"{target}[{index}] = {value}"]

    def _stmt_ReturnStatement(self, node: Any, depth: int) -> List[str]:
        value = getattr(node, "value", None)
        if value is None:
            return ["return None"]
        return [f"return {self._gen_expr(value)}"]

    def _stmt_PrintStatement(self, node: Any, depth: int) -> List[str]:
        return self._stmt_print_alias(node, depth)

    def _stmt_print_alias(self, node: Any, depth: int) -> List[str]:
        val = (
            getattr(node, "value", None)
            or getattr(node, "expression", None)
            or getattr(node, "text", None)
        )
        return [f"_print({self._gen_expr(val)})"]

    def _stmt_IfStatement(self, node: Any, depth: int) -> List[str]:
        cond = self._gen_expr(getattr(node, "condition", None))
        then_block = list(getattr(node, "then_block", None) or [])
        else_block = list(
            getattr(node, "else_block", None)
            or getattr(node, "else_body", None)
            or []
        )
        lines: List[str] = [f"if {cond}:"]
        if then_block:
            for sub in then_block:
                for ln in self._gen_stmt(sub, depth + 1):
                    lines.append("    " + ln)
        else:
            lines.append("    pass")

        # elif branches (optional attribute)
        for elif_node in (getattr(node, "elif_branches", None) or []):
            elif_cond = self._gen_expr(getattr(elif_node, "condition", None))
            lines.append(f"elif {elif_cond}:")
            for sub in (getattr(elif_node, "body", None) or []):
                for ln in self._gen_stmt(sub, depth + 1):
                    lines.append("    " + ln)

        if else_block:
            lines.append("else:")
            for sub in else_block:
                for ln in self._gen_stmt(sub, depth + 1):
                    lines.append("    " + ln)
        return lines

    def _stmt_WhileLoop(self, node: Any, depth: int) -> List[str]:
        cond = self._gen_expr(getattr(node, "condition", None))
        body = list(getattr(node, "body", None) or [])
        lines: List[str] = [f"while {cond}:"]
        if body:
            for sub in body:
                for ln in self._gen_stmt(sub, depth + 1):
                    lines.append("    " + ln)
        else:
            lines.append("    pass")
        return lines

    def _stmt_ForLoop(self, node: Any, depth: int) -> List[str]:
        iterator = getattr(node, "iterator", "_it")
        start = getattr(node, "start", None)
        end = getattr(node, "end", None)
        step = getattr(node, "step", None)
        iterable = getattr(node, "iterable", None)
        body = list(getattr(node, "body", None) or [])

        if start is not None and end is not None:
            s = self._gen_expr(start)
            e = self._gen_expr(end)
            if step is not None:
                range_expr = f"range({s}, {e}, {self._gen_expr(step)})"
            else:
                range_expr = f"range({s}, {e})"
            lines: List[str] = [f"for {iterator} in {range_expr}:"]
        elif iterable is not None:
            it_expr = (
                iterable if isinstance(iterable, str) else self._gen_expr(iterable)
            )
            lines = [f"for {iterator} in {it_expr}:"]
        else:
            lines = [f"for {iterator} in []:  # unknown ForLoop iterable"]

        if body:
            for sub in body:
                for ln in self._gen_stmt(sub, depth + 1):
                    lines.append("    " + ln)
        else:
            lines.append("    pass")
        return lines

    def _stmt_RepeatNTimesLoop(self, node: Any, depth: int) -> List[str]:
        count = getattr(node, "count", None)
        body = list(getattr(node, "body", None) or [])
        lines: List[str] = [f"for _repeat_idx in range({self._gen_expr(count)}):"]
        if body:
            for sub in body:
                for ln in self._gen_stmt(sub, depth + 1):
                    lines.append("    " + ln)
        else:
            lines.append("    pass")
        return lines

    def _stmt_RepeatWhileLoop(self, node: Any, depth: int) -> List[str]:
        cond = self._gen_expr(getattr(node, "condition", None))
        body = list(getattr(node, "body", None) or [])
        lines: List[str] = [f"while {cond}:"]
        if body:
            for sub in body:
                for ln in self._gen_stmt(sub, depth + 1):
                    lines.append("    " + ln)
        else:
            lines.append("    pass")
        return lines

    def _stmt_BreakStatement(self, node: Any, depth: int) -> List[str]:
        return ["break"]

    def _stmt_ContinueStatement(self, node: Any, depth: int) -> List[str]:
        return ["continue"]

    def _stmt_FunctionCall(self, node: Any, depth: int) -> List[str]:
        return [self._gen_expr(node)]

    def _stmt_ExpressionStatement(self, node: Any, depth: int) -> List[str]:
        expr = getattr(node, "expression", None) or getattr(node, "value", None)
        if expr is None:
            return ["pass"]
        return [self._gen_expr(expr)]

    # ------------------------------------------------------------------
    # Expression translation
    # ------------------------------------------------------------------

    def _gen_expr(self, node: Any) -> str:
        """Translate an expression AST node into a Python expression string."""
        if node is None:
            return "None"
        # Python primitives can appear directly (e.g. from strength-reduction)
        if isinstance(node, bool):
            return "True" if node else "False"
        if isinstance(node, int):
            return repr(node)
        if isinstance(node, float):
            return repr(node)
        if isinstance(node, str):
            return repr(node)

        kind = type(node).__name__
        method = getattr(self, f"_expr_{kind}", None)
        if method is not None:
            return method(node)
        return f"None  # unhandled expression: {kind}"

    def _expr_Literal(self, node: Any) -> str:
        val = getattr(node, "value", None)
        if val is None:
            return "None"
        if isinstance(val, bool):
            return "True" if val else "False"
        return repr(val)

    # SyntheticLiteral produced by loop strength-reduction
    def _expr__SyntheticLiteral(self, node: Any) -> str:
        return repr(getattr(node, "value", 0))

    def _expr_Identifier(self, node: Any) -> str:
        name = getattr(node, "name", "_unknown")
        # Map NexusLang boolean / null literals to Python equivalents
        if name in ("true", "True"):
            return "True"
        if name in ("false", "False"):
            return "False"
        if name in ("null", "nil", "None"):
            return "None"
        return name

    def _expr_BinaryOperation(self, node: Any) -> str:
        op = getattr(node, "operator", "+")
        py_op = _OP_MAP.get(op, op)
        left = self._gen_expr(getattr(node, "left", None))
        right = self._gen_expr(getattr(node, "right", None))
        return f"({left} {py_op} {right})"

    def _expr_UnaryOperation(self, node: Any) -> str:
        op = getattr(node, "operator", "-")
        py_op = {"-": "-", "not": "not ", "~": "~"}.get(op, op)
        operand = self._gen_expr(getattr(node, "operand", None))
        return f"({py_op}{operand})"

    def _expr_FunctionCall(self, node: Any) -> str:
        name = getattr(node, "name", None) or getattr(node, "callee", "unknown")
        if not isinstance(name, str):
            name_str = self._gen_expr(name)
            # name must be a string for _call; wrap in an eval if it's computed
            return f"_call({name_str}, [{', '.join(self._gen_expr(a) for a in (getattr(node, 'arguments', None) or []))}])"
        args = list(getattr(node, "arguments", None) or [])
        arg_exprs = ", ".join(self._gen_expr(a) for a in args)
        return f"_call({repr(name)}, [{arg_exprs}])"

    def _expr_IndexAccess(self, node: Any) -> str:
        target = self._gen_expr(getattr(node, "target", None))
        index = self._gen_expr(getattr(node, "index", None))
        return f"({target}[{index}])"

    def _expr_IndexExpression(self, node: Any) -> str:
        arr = self._gen_expr(getattr(node, "array_expr", None))
        idx = self._gen_expr(getattr(node, "index_expr", None))
        return f"({arr}[{idx}])"

    def _expr_ListLiteral(self, node: Any) -> str:
        elements = list(getattr(node, "elements", None) or [])
        return "[" + ", ".join(self._gen_expr(e) for e in elements) + "]"

    def _expr_ListExpression(self, node: Any) -> str:
        return self._expr_ListLiteral(node)

    def _expr_DictLiteral(self, node: Any) -> str:
        # Support both (pairs list) and (keys + values lists)
        pairs = list(
            getattr(node, "pairs", None) or getattr(node, "items", None) or []
        )
        if pairs:
            parts = []
            for p in pairs:
                if isinstance(p, (list, tuple)) and len(p) == 2:
                    k, v = p
                else:
                    k = getattr(p, "key", None)
                    v = getattr(p, "value", None)
                parts.append(f"{self._gen_expr(k)}: {self._gen_expr(v)}")
            return "{" + ", ".join(parts) + "}"
        keys = list(getattr(node, "keys", None) or [])
        vals = list(getattr(node, "values", None) or [])
        parts = [
            f"{self._gen_expr(k)}: {self._gen_expr(v)}"
            for k, v in zip(keys, vals)
        ]
        return "{" + ", ".join(parts) + "}"

    def _expr_DictExpression(self, node: Any) -> str:
        # DictExpression.entries is a list of (key_expr, value_expr) tuples
        entries = list(getattr(node, "entries", None) or [])
        parts = []
        for entry in entries:
            if isinstance(entry, (list, tuple)) and len(entry) >= 2:
                k, v = entry[0], entry[1]
            else:
                k = getattr(entry, "key", None)
                v = getattr(entry, "value", None)
            parts.append(f"{self._gen_expr(k)}: {self._gen_expr(v)}")
        return "{" + ", ".join(parts) + "}"

    def _expr_TernaryExpression(self, node: Any) -> str:
        cond = self._gen_expr(getattr(node, "condition", None))
        then_val = self._gen_expr(getattr(node, "then_value", None))
        else_val = self._gen_expr(getattr(node, "else_value", None))
        return f"({then_val} if {cond} else {else_val})"

    def _expr_StringInterpolation(self, node: Any) -> str:
        parts = list(getattr(node, "parts", None) or [])
        if not parts:
            return repr("")
        fragments: List[str] = []
        for p in parts:
            if isinstance(p, str):
                fragments.append(repr(p))
            elif type(p).__name__ == "Literal":
                fragments.append(repr(getattr(p, "value", "")))
            else:
                fragments.append(f"str({self._gen_expr(p)})")
        return "(" + " + ".join(fragments) + ")"

    def _expr_FStringExpression(self, node: Any) -> str:
        # FStringExpression.parts is a list of (is_literal, content, format_spec) tuples
        parts = list(getattr(node, "parts", None) or [])
        if not parts:
            return repr("")
        fragments: List[str] = []
        for p in parts:
            if isinstance(p, (list, tuple)) and len(p) >= 2:
                is_literal, content = p[0], p[1]
                if is_literal:
                    fragments.append(repr(str(content)))
                else:
                    fragments.append(f"str({self._gen_expr(content)})")
            elif isinstance(p, str):
                fragments.append(repr(p))
            else:
                fragments.append(f"str({self._gen_expr(p)})")
        if len(fragments) == 1:
            return fragments[0]
        return "(" + " + ".join(fragments) + ")"

    def _expr_MemberAccess(self, node: Any) -> str:
        # MemberAccess uses object_expr and member_name attributes
        obj_node = getattr(node, "object_expr", None) or getattr(node, "object", None)
        obj = self._gen_expr(obj_node)
        member = getattr(node, "member_name", None) or getattr(node, "member", "_unknown")
        return f"{obj}.{member}"


# ---------------------------------------------------------------------------
# Helper factories — build _call / _print for the generated code namespace
# ---------------------------------------------------------------------------

def _make_call_helper(interpreter: Any) -> Callable:
    """
    Build a ``_call(name: str, args: list) -> Any`` function that dispatches
    NexusLang function calls through the interpreter.

    The interpreter is captured in a closure so that the generated function
    does not need a reference to it at call time.
    """
    call_fn = getattr(interpreter, "call_function", None)
    if call_fn is not None:
        def _call(name: str, args: list) -> Any:
            return call_fn(name, args)
        return _call

    # Fallback: try the interpreter's execute() with a synthetic FunctionCall
    execute_fn = getattr(interpreter, "execute", None)

    def _call_fallback(name: str, args: list) -> Any:
        if execute_fn is not None:
            import types as _t
            node = _t.SimpleNamespace()
            node.__class__ = type("FunctionCall", (), {})  # type: ignore[assignment]
            node.name = name
            node.arguments = args
            return execute_fn(node)
        return None

    return _call_fallback


def _make_print_helper(interpreter: Any) -> Callable:
    """
    Build a ``_print(value) -> None`` function for the generated code.
    Uses the interpreter's runtime print if available, otherwise falls back
    to Python's built-in print().
    """
    runtime = getattr(interpreter, "runtime", None)
    if runtime is not None:
        print_fn = getattr(runtime, "print_value", None)
        if print_fn is not None:
            return print_fn  # type: ignore[return-value]
    return print  # type: ignore[return-value]
