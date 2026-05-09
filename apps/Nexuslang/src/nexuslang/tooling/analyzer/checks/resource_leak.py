"""Resource leak checker.

Detects:
- R001: Reacquire resource without releasing previous one
- R002: Release non-existent resource
- R003: Release type mismatch
- R004: Double release
- R005: Resource unreleased at end of scope
- R006: Resource unreleased on return
- R007: Potential lock deadlock from nested lock acquisition
- R008: Connection operation without timeout
- R009: Cascade leak in loops (resource acquired repeatedly without release)
- R010: Exception path leak in try/catch blocks
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Any, Dict, List, Optional, Set, Tuple
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../../../.."))

from nexuslang.parser.ast import ASTNode
from .base import BaseChecker
from ..report import Category, Issue, Severity, SourceLocation


@dataclass
class ResourceInfo:
    var_name: str
    resource_type: str
    location: SourceLocation
    is_released: bool = False
    released_location: Optional[SourceLocation] = None


class ResourceLeakChecker(BaseChecker):
    """Checks for resource leaks and resource lifecycle misuse."""

    CHECKER_NAME = "resource_leak"

    ACQUIRE_FUNCS = {
        "open": "file",
        "open_file": "file",
        "file_open": "file",
        "alloc": "memory",
        "allocate": "memory",
        "malloc": "memory",
        "lock": "lock",
        "acquire_lock": "lock",
        "connect": "connection",
        "open_connection": "connection",
    }

    RELEASE_FUNCS = {
        "close": "file",
        "close_file": "file",
        "file_close": "file",
        "dealloc": "memory",
        "free": "memory",
        "unlock": "lock",
        "release_lock": "lock",
        "disconnect": "connection",
        "close_connection": "connection",
    }

    LOOP_NODE_TYPES = {"ForLoop", "WhileLoop", "RepeatNTimesLoop", "RepeatWhileLoop"}

    def __init__(self) -> None:
        super().__init__()
        self.resources: Dict[str, ResourceInfo] = {}

    def check(self, ast: ASTNode, source: str, lines: List[str]) -> List[Issue]:
        self.issues = []
        self.current_source = source
        self.current_lines = lines
        self.resources = {}

        self._walk_nodes(ast, self._check_node, set())
        self._check_leaks()
        return self.issues

    def _walk_nodes(self, node: Any, callback, seen: Set[int]) -> None:
        if node is None:
            return
        if isinstance(node, (str, bytes, int, float, bool)):
            return

        node_id = id(node)
        if node_id in seen:
            return
        seen.add(node_id)

        callback(node)

        if isinstance(node, dict):
            for value in node.values():
                self._walk_nodes(value, callback, seen)
            return

        if isinstance(node, (list, tuple, set)):
            for item in node:
                self._walk_nodes(item, callback, seen)
            return

        if hasattr(node, "__dict__"):
            for value in vars(node).values():
                self._walk_nodes(value, callback, seen)

    def _check_node(self, node: ASTNode) -> None:
        node_type = type(node).__name__

        if node_type == "VariableDeclaration":
            self._check_acquisition(node)
        elif node_type == "FunctionCall":
            self._check_release(node)
            self._check_lock_deadlock(node)
            self._check_connection_timeout(node)
        elif node_type in self.LOOP_NODE_TYPES:
            self._check_cascade_leak(node)
        elif node_type in {"TryCatch", "TryCatchBlock"}:
            self._check_exception_path_leak(node)
        elif node_type == "ReturnStatement":
            self._check_leaks_on_return(node)

    def _check_acquisition(self, node: ASTNode) -> None:
        value = getattr(node, "value", None)
        if type(value).__name__ != "FunctionCall":
            return

        func_name = getattr(value, "name", None)
        if func_name not in self.ACQUIRE_FUNCS:
            return

        resource_type = self.ACQUIRE_FUNCS[func_name]
        var_name = getattr(node, "name", None) or getattr(node, "identifier", None)
        if not var_name:
            return

        location = self.get_node_location(node)

        if var_name in self.resources and not self.resources[var_name].is_released:
            old_resource = self.resources[var_name]
            self.issues.append(Issue(
                code="R001",
                severity=Severity.WARNING,
                category=Category.RESOURCE_LEAK,
                message=(
                    f"Resource leak: '{var_name}' ({old_resource.resource_type}) "
                    "not released before reacquisition"
                ),
                location=location,
                source_line=self.get_source_line(location.line),
                suggestion=self._get_release_suggestion(old_resource),
                related_locations=[old_resource.location],
            ))

        self.resources[var_name] = ResourceInfo(
            var_name=var_name,
            resource_type=resource_type,
            location=location,
        )

    def _check_release(self, node: ASTNode) -> None:
        func_name = getattr(node, "name", None)
        if func_name not in self.RELEASE_FUNCS:
            return

        resource_type = self.RELEASE_FUNCS[func_name]
        args = getattr(node, "arguments", [])
        if not args:
            return

        var_name = self._extract_identifier_name(args[0])
        if not var_name:
            return

        location = self.get_node_location(node)

        if var_name not in self.resources:
            self.issues.append(Issue(
                code="R002",
                severity=Severity.WARNING,
                category=Category.RESOURCE_LEAK,
                message=f"Attempting to release non-existent {resource_type}: '{var_name}'",
                location=location,
                source_line=self.get_source_line(location.line),
                suggestion=f"Ensure '{var_name}' is acquired before releasing",
            ))
            return

        resource = self.resources[var_name]
        if resource.resource_type != resource_type:
            self.issues.append(Issue(
                code="R003",
                severity=Severity.ERROR,
                category=Category.RESOURCE_LEAK,
                message=(
                    f"Type mismatch: trying to release {resource_type} but "
                    f"'{var_name}' is {resource.resource_type}"
                ),
                location=location,
                source_line=self.get_source_line(location.line),
                suggestion=f"Use correct release function for {resource.resource_type}",
            ))
            return

        if resource.is_released:
            self.issues.append(Issue(
                code="R004",
                severity=Severity.ERROR,
                category=Category.RESOURCE_LEAK,
                message=f"Double-release: {resource_type} '{var_name}' already released",
                location=location,
                source_line=self.get_source_line(location.line),
                suggestion="Remove duplicate release",
                related_locations=[resource.released_location] if resource.released_location else [],
            ))
            return

        resource.is_released = True
        resource.released_location = location

    def _check_leaks(self) -> None:
        for var_name, resource in self.resources.items():
            if not resource.is_released:
                self.issues.append(Issue(
                    code="R005",
                    severity=Severity.WARNING,
                    category=Category.RESOURCE_LEAK,
                    message=f"Resource leak: {resource.resource_type} '{var_name}' not released",
                    location=resource.location,
                    source_line=self.get_source_line(resource.location.line),
                    suggestion=self._get_release_suggestion(resource),
                    fix=self._get_release_fix(resource),
                ))

    def _check_leaks_on_return(self, node: ASTNode) -> None:
        location = self.get_node_location(node)
        for var_name, resource in self.resources.items():
            if not resource.is_released:
                self.issues.append(Issue(
                    code="R006",
                    severity=Severity.WARNING,
                    category=Category.RESOURCE_LEAK,
                    message=(
                        f"Resource leak on return: {resource.resource_type} "
                        f"'{var_name}' not released"
                    ),
                    location=location,
                    source_line=self.get_source_line(location.line),
                    suggestion=self._get_release_suggestion(resource),
                    related_locations=[resource.location],
                ))

    def _check_lock_deadlock(self, node: ASTNode) -> None:
        func_name = getattr(node, "name", None)
        if func_name != "lock":
            return

        held_locks = [
            r for r in self.resources.values()
            if r.resource_type == "lock" and not r.is_released
        ]
        if not held_locks:
            return

        lock_name = None
        args = getattr(node, "arguments", [])
        if args:
            lock_name = self._extract_identifier_name(args[0]) or "<unknown-lock>"

        location = self.get_node_location(node)
        self.issues.append(Issue(
            code="R007",
            severity=Severity.WARNING,
            category=Category.RESOURCE_LEAK,
            message=(
                f"Potential deadlock: acquiring lock '{lock_name}' "
                f"while holding {len(held_locks)} lock(s)"
            ),
            location=location,
            source_line=self.get_source_line(location.line),
            suggestion="Acquire locks in a globally consistent order",
            related_locations=[r.location for r in held_locks],
        ))

    def _check_connection_timeout(self, node: ASTNode) -> None:
        func_name = getattr(node, "name", None)
        if func_name not in {"connect", "open_connection"}:
            return

        args = getattr(node, "arguments", [])
        # Common positional forms pass timeout as the second argument.
        if len(args) >= 2:
            return

        has_timeout = any(
            isinstance(getattr(arg, "name", None), str)
            and "timeout" in getattr(arg, "name", "").lower()
            for arg in args
        )
        if has_timeout:
            return

        location = self.get_node_location(node)
        self.issues.append(Issue(
            code="R008",
            severity=Severity.WARNING,
            category=Category.RESOURCE_LEAK,
            message=f"Connection operation '{func_name}' has no timeout and may block indefinitely",
            location=location,
            source_line=self.get_source_line(location.line),
            suggestion=f"Add timeout argument to '{func_name}'",
        ))

    def _check_cascade_leak(self, loop_node: ASTNode) -> None:
        """R009: detect repeated resource acquisition in loop body without release."""
        loop_body = self._extract_loop_body(loop_node)
        if not loop_body:
            return

        acquisitions, releases = self._collect_acquire_release_names(loop_body)
        unreleased = [name for name in acquisitions if name not in releases]
        if not unreleased:
            return

        location = self.get_node_location(loop_node)
        for resource_name in sorted(set(unreleased)):
            self.issues.append(Issue(
                code="R009",
                severity=Severity.WARNING,
                category=Category.RESOURCE_LEAK,
                message=(
                    f"Cascade leak risk: resource '{resource_name}' is acquired in a loop "
                    "without a matching release in loop body"
                ),
                location=location,
                source_line=self.get_source_line(location.line),
                suggestion=f"Release '{resource_name}' inside the loop iteration",
            ))

    def _check_exception_path_leak(self, node: ASTNode) -> None:
        """R010: detect resources acquired in try path but not released in catch path."""
        try_block = getattr(node, "try_block", None)
        catch_block = getattr(node, "catch_block", None)

        if not try_block:
            return

        try_acq, _ = self._collect_acquire_release_names(try_block)
        catch_guaranteed_rel = self._guaranteed_releases_on_all_paths(catch_block or [])
        try_guaranteed_rel = self._guaranteed_releases_on_all_paths(try_block)

        # Exception-safe if catch guarantees release, or if both try/catch guarantee release.
        leaked_on_exception = [
            name for name in try_acq
            if name not in catch_guaranteed_rel and name not in try_guaranteed_rel
        ]
        if not leaked_on_exception:
            return

        location = self.get_node_location(node)
        for resource_name in sorted(set(leaked_on_exception)):
            self.issues.append(Issue(
                code="R010",
                severity=Severity.WARNING,
                category=Category.RESOURCE_LEAK,
                message=(
                    f"Exception-path leak risk: resource '{resource_name}' acquired in try block "
                    "is not guaranteed to be released in catch path"
                ),
                location=location,
                source_line=self.get_source_line(location.line),
                suggestion="Ensure resource release occurs in both success and failure paths",
            ))

    def _guaranteed_releases_on_all_paths(self, subtree: Any) -> Set[str]:
        """Return names guaranteed to be released regardless of path within subtree."""
        if subtree is None:
            return set()

        if isinstance(subtree, list):
            guaranteed: Set[str] = set()
            for statement in subtree:
                guaranteed |= self._guaranteed_releases_on_all_paths(statement)
            return guaranteed

        node_type = type(subtree).__name__

        if node_type == "FunctionCall":
            func_name = getattr(subtree, "name", None)
            if func_name in self.RELEASE_FUNCS:
                args = getattr(subtree, "arguments", [])
                if args:
                    name = self._extract_identifier_name(args[0])
                    return {name} if name else set()
            return set()

        if node_type == "IfStatement":
            then_block = getattr(subtree, "then_block", []) or []
            else_block = getattr(subtree, "else_block", []) or []
            if not else_block:
                return set()
            then_rel = self._guaranteed_releases_on_all_paths(then_block)
            else_rel = self._guaranteed_releases_on_all_paths(else_block)
            return then_rel & else_rel

        if node_type in {"TryCatch", "TryCatchBlock"}:
            try_rel = self._guaranteed_releases_on_all_paths(getattr(subtree, "try_block", []) or [])
            catch_rel = self._guaranteed_releases_on_all_paths(getattr(subtree, "catch_block", []) or [])
            return try_rel & catch_rel

        if node_type in self.LOOP_NODE_TYPES:
            return set()

        if hasattr(subtree, "__dict__"):
            combined: Set[str] = set()
            for value in vars(subtree).values():
                if isinstance(value, (list, tuple, set)):
                    combined |= self._guaranteed_releases_on_all_paths(list(value))
                elif hasattr(value, "__dict__"):
                    combined |= self._guaranteed_releases_on_all_paths(value)
            return combined

        return set()

    def _collect_acquire_release_names(self, subtree: Any) -> Tuple[Set[str], Set[str]]:
        acquisitions: Set[str] = set()
        releases: Set[str] = set()

        def visit(node: Any) -> None:
            node_type = type(node).__name__
            if node_type == "VariableDeclaration":
                value = getattr(node, "value", None)
                if type(value).__name__ != "FunctionCall":
                    return
                func_name = getattr(value, "name", None)
                if func_name in self.ACQUIRE_FUNCS:
                    name = getattr(node, "name", None) or getattr(node, "identifier", None)
                    if name:
                        acquisitions.add(name)
            elif node_type == "FunctionCall":
                func_name = getattr(node, "name", None)
                if func_name in self.RELEASE_FUNCS:
                    args = getattr(node, "arguments", [])
                    if args:
                        name = self._extract_identifier_name(args[0])
                        if name:
                            releases.add(name)

        self._walk_nodes(subtree, visit, set())
        return acquisitions, releases

    def _extract_loop_body(self, loop_node: ASTNode) -> List[Any]:
        for attr in ("body", "statements", "then_block"):
            value = getattr(loop_node, attr, None)
            if isinstance(value, list):
                return value
        return []

    def _extract_identifier_name(self, expr: Any) -> Optional[str]:
        if expr is None:
            return None
        expr_type = type(expr).__name__
        if expr_type in {"Identifier", "VariableReference"}:
            return getattr(expr, "name", None)
        if expr_type == "Literal":
            value = getattr(expr, "value", None)
            if isinstance(value, str):
                return value
        return None

    def _get_release_suggestion(self, resource: ResourceInfo) -> str:
        suggestions = {
            "file": f"Call 'close {resource.var_name}' before leaving scope",
            "memory": f"Call 'dealloc {resource.var_name}' before leaving scope",
            "lock": f"Call 'unlock {resource.var_name}' before leaving scope",
            "connection": f"Call 'disconnect {resource.var_name}' before leaving scope",
        }
        return suggestions.get(resource.resource_type, f"Release {resource.var_name}")

    def _get_release_fix(self, resource: ResourceInfo) -> str:
        fixes = {
            "file": f"call close with {resource.var_name}",
            "memory": f"call dealloc with {resource.var_name}",
            "lock": f"call unlock with {resource.var_name}",
            "connection": f"call disconnect with {resource.var_name}",
        }
        return fixes.get(resource.resource_type, "")
