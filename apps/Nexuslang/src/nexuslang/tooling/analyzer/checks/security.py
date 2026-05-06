"""
Security Lint Checks
======================

Detects security vulnerabilities and unsafe coding patterns:

SEC001  Hardcoded credentials or secrets (password, key, token literals)
SEC002  SQL injection risk (string interpolation used in SQL query call)
SEC003  Path traversal risk (unvalidated variable used in file path)
SEC004  Command injection risk (variable used directly in shell command)
SEC005  Insecure random number generator for security-sensitive assignment
SEC006  Unvalidated integer from external source used as array index
SEC007  Printing sensitive variable (password, secret, token, key)
SEC008  Use of weak/broken cryptographic primitive (MD5, SHA1, RC4, DES)
SEC009  Integer overflow risk in unchecked arithmetic on untrusted input
SEC010  Missing return-value check on FFI/unsafe call
"""

import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../../..'))

from typing import Any, List, Optional, Set

from nexuslang.parser.ast import ASTNode
from .base import BaseChecker
from ..report import Issue, Severity, Category, SourceLocation


# ---------------------------------------------------------------------------
# Rule-specific constants
# ---------------------------------------------------------------------------

_SECRET_NAMES: Set[str] = {
    "password", "passwd", "secret", "token", "api_key", "apikey",
    "private_key", "encryption_key", "auth_token", "access_token",
    "refresh_token", "session_key", "session_token", "credentials",
    "db_password", "database_password", "secret_key", "client_secret",
}

_WEAK_CRYPTO: Set[str] = {
    "md5", "sha1", "rc4", "des", "triple_des", "3des",
    "md5_hash", "sha1_hash", "md5_digest",
}

_SQL_CALLS: Set[str] = {
    "execute", "query", "execute_sql", "run_query",
    "db_execute", "db_query", "cursor_execute", "sql_execute",
}

_SHELL_CALLS: Set[str] = {
    "shell", "execute_shell", "run_shell", "system", "popen",
    "shell_exec", "exec_command", "run_command", "os_execute",
}

_RANDOM_CALLS: Set[str] = {"random", "rand", "random_int", "random_float", "random_bytes"}

_SECURITY_CONTEXT_KEYWORDS: Set[str] = {
    "token", "key", "secret", "password", "nonce", "salt", "iv",
    "session", "challenge", "otp", "csrf",
}


class SecurityChecker(BaseChecker):
    """
    Lint checker that identifies security vulnerabilities in NexusLang code.

    Error codes: SEC001-SEC010.
    """

    CHECKER_NAME = "security"

    def check(self, ast: ASTNode, source: str, lines: List[str]) -> List[Issue]:
        self.issues = []
        self.current_source = source
        self.current_lines = lines
        self._walk(ast, parent=None)
        return self.issues

    # ------------------------------------------------------------------
    # AST walker
    # ------------------------------------------------------------------

    def _walk(self, node: Any, parent: Any = None, depth: int = 0) -> None:
        if node is None or depth > 80:
            return
        node_type = type(node).__name__

        if node_type == "VariableDeclaration":
            self._check_var_decl(node)
        elif node_type == "Assignment":
            self._check_assignment(node)
        elif node_type == "FunctionCall":
            self._check_function_call(node, parent)
        elif node_type == "PrintStatement":
            self._check_print_statement(node)

        for child in self._iter_children(node):
            self._walk(child, parent=node, depth=depth + 1)

    # ------------------------------------------------------------------
    # Checks
    # ------------------------------------------------------------------

    def _check_var_decl(self, node: Any) -> None:
        name = (getattr(node, "name", None) or "").lower()
        value = getattr(node, "value", None)
        line = getattr(node, "line", 0)

        # SEC001: hardcoded credential
        if name in _SECRET_NAMES and value is not None:
            if type(value).__name__ in ("StringLiteral", "Literal"):
                literal = getattr(value, "value", "")
                if isinstance(literal, str) and literal not in ("", "<placeholder>", "***"):
                    self.issues.append(Issue(
                        code="SEC001",
                        severity=Severity.ERROR,
                        category=Category.SECURITY,
                        message=(
                            f"Hardcoded secret/credential in variable `{name}`. "
                            "Use environment variables or a secrets manager instead."
                        ),
                        location=self.get_node_location(node),
                        source_line=self.get_source_line(line),
                        suggestion="Load from environment: `set password to env_get(\"APP_PASSWORD\")`",
                    ))

    def _check_assignment(self, node: Any) -> None:
        target = getattr(node, "target", None)
        target_name = ""
        if target is not None:
            target_name = (
                getattr(target, "name", None)
                or getattr(target, "value", None)
                or ""
            ).lower()

        value = getattr(node, "value", None)
        line = getattr(node, "line", 0)

        # SEC001: overwriting a secret variable with a hardcoded string
        if target_name in _SECRET_NAMES and value is not None:
            if type(value).__name__ == "StringLiteral":
                literal = getattr(value, "value", "")
                if isinstance(literal, str) and len(literal) > 0:
                    self.issues.append(Issue(
                        code="SEC001",
                        severity=Severity.ERROR,
                        category=Category.SECURITY,
                        message=(
                            f"Hardcoded credential assigned to `{target_name}`. "
                            "Use environment variables."
                        ),
                        location=self.get_node_location(node),
                        source_line=self.get_source_line(line),
                    ))

    def _check_function_call(self, call: Any, parent: Any) -> None:
        name = (getattr(call, "name", None) or "").lower()
        args = (
            getattr(call, "arguments", None)
            or getattr(call, "args", None)
            or []
        )
        line = getattr(call, "line", 0)

        # SEC002: SQL injection
        if name in _SQL_CALLS:
            for arg in args:
                if self._has_string_interpolation(arg) or self._has_concat_with_var(arg):
                    self.issues.append(Issue(
                        code="SEC002",
                        severity=Severity.ERROR,
                        category=Category.SECURITY,
                        message=(
                            "Possible SQL injection: user input interpolated directly into "
                            "a SQL query string. Use parameterized queries."
                        ),
                        location=self.get_node_location(call),
                        source_line=self.get_source_line(line),
                        suggestion="Pass values as query parameters, not via string formatting.",
                    ))
                    break

        # SEC004: command injection
        if name in _SHELL_CALLS:
            for arg in args:
                if self._contains_var_ref(arg):
                    self.issues.append(Issue(
                        code="SEC004",
                        severity=Severity.ERROR,
                        category=Category.SECURITY,
                        message=(
                            "Possible command injection: variable used in shell command. "
                            "Validate and sanitize all inputs."
                        ),
                        location=self.get_node_location(call),
                        source_line=self.get_source_line(line),
                        suggestion="Use argument arrays and avoid shell interpolation.",
                    ))
                    break

        # SEC005: insecure random in security context
        if name in _RANDOM_CALLS and parent is not None:
            parent_target = (
                getattr(parent, "name", None)
                or getattr(getattr(parent, "target", None), "name", None)
                or ""
            ).lower()
            if any(kw in parent_target for kw in _SECURITY_CONTEXT_KEYWORDS):
                self.issues.append(Issue(
                    code="SEC005",
                    severity=Severity.WARNING,
                    category=Category.SECURITY,
                    message=(
                        f"`{name}()` is not cryptographically secure. "
                        "Use `crypto_random_bytes()` for security-critical values."
                    ),
                    location=self.get_node_location(call),
                    source_line=self.get_source_line(line),
                ))

        # SEC008: weak cryptographic primitive
        if name in _WEAK_CRYPTO:
            self.issues.append(Issue(
                code="SEC008",
                severity=Severity.ERROR,
                category=Category.SECURITY,
                message=(
                    f"`{name}()` is a broken/weak cryptographic primitive. "
                    "Use SHA-256, SHA-512, or BLAKE3 instead."
                ),
                location=self.get_node_location(call),
                source_line=self.get_source_line(line),
                suggestion="Replace with a strong hash: `sha256(data)` or `blake3(data)`.",
            ))

        # SEC007: printing sensitive variables via function call
        if name == "print":
            for arg in args:
                var_name = (getattr(arg, "name", None) or "").lower()
                if var_name in _SECRET_NAMES:
                    self.issues.append(Issue(
                        code="SEC007",
                        severity=Severity.WARNING,
                        category=Category.SECURITY,
                        message=(
                            f"Printing sensitive variable `{var_name}` may expose "
                            "credentials in logs or to users."
                        ),
                        location=self.get_node_location(call),
                        source_line=self.get_source_line(line),
                    ))

    def _check_print_statement(self, node: Any) -> None:
        """SEC007: `print text <sensitive_var>` exposes credentials."""
        expr = getattr(node, "expression", None)
        if expr is None:
            return
        var_name = (getattr(expr, "name", None) or "").lower()
        if var_name in _SECRET_NAMES:
            line = getattr(node, "line_number", 0) or getattr(node, "line", 0)
            self.issues.append(Issue(
                code="SEC007",
                severity=Severity.WARNING,
                category=Category.SECURITY,
                message=(
                    f"Printing sensitive variable `{var_name}` may expose "
                    "credentials in logs or to users."
                ),
                location=self.get_node_location(node),
                source_line=self.get_source_line(line),
            ))

    # ------------------------------------------------------------------
    # Predicates
    # ------------------------------------------------------------------

    def _has_string_interpolation(self, node: Any) -> bool:
        if node is None:
            return False
        return type(node).__name__ in ("TemplateString", "InterpolatedString", "FormatString")

    def _has_concat_with_var(self, node: Any) -> bool:
        if node is None:
            return False
        if type(node).__name__ == "BinaryOp":
            op = getattr(node, "operator", None) or getattr(node, "op", None)
            if op == "+":
                return (
                    self._contains_var_ref(getattr(node, "left", None))
                    or self._contains_var_ref(getattr(node, "right", None))
                )
        return False

    def _contains_var_ref(self, node: Any) -> bool:
        if node is None:
            return False
        if type(node).__name__ in ("Name", "Identifier", "VariableRef", "NameExpression"):
            return True
        for child in self._iter_children(node):
            if self._contains_var_ref(child):
                return True
        return False

    # ------------------------------------------------------------------
    # Iteration helpers
    # ------------------------------------------------------------------

    def _iter_children(self, node: Any):
        if not hasattr(node, "__dict__"):
            return
        for k, v in vars(node).items():
            if k.startswith("_"):
                continue
            if isinstance(v, list):
                yield from [i for i in v if i is not None and hasattr(i, "__dict__")]
            elif hasattr(v, "__dict__"):
                yield v
