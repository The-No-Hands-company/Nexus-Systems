"""Memory safety checker.

Detects:
- M001: Reassign without freeing previous allocation
- M002: Free unallocated memory
- M003: Double-free
- M004: Realloc unallocated memory
- M005: Realloc freed memory
- M006: Use-after-free
- M007: Unfreed allocation at end of scope
- M008: Unfreed allocation on return
- M009: Out-of-bounds index access (bounds checking)
- M010: Null pointer dereference
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Any, Dict, List, Optional, Set
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../../../.."))

from nexuslang.parser.ast import ASTNode
from .base import BaseChecker
from ..report import Category, Issue, Severity, SourceLocation


@dataclass
class AllocationInfo:
    var_name: str
    size: Optional[int]
    location: SourceLocation
    is_freed: bool = False
    freed_location: Optional[SourceLocation] = None


class MemorySafetyChecker(BaseChecker):
    """Checks for memory-safety violations."""

    CHECKER_NAME = "memory_safety"

    def __init__(self) -> None:
        super().__init__()
        self.allocations: Dict[str, AllocationInfo] = {}
        self.pointer_aliases: Dict[str, str] = {}
        self.known_container_sizes: Dict[str, int] = {}
        self.nullable_vars: Set[str] = set()
        self.in_function = False

    def check(self, ast: ASTNode, source: str, lines: List[str]) -> List[Issue]:
        self.issues = []
        self.current_source = source
        self.current_lines = lines
        self.allocations = {}
        self.pointer_aliases = {}
        self.known_container_sizes = {}
        self.nullable_vars = set()
        self.in_function = False

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

        node_type = type(node).__name__
        function_scope_state = None
        if node_type == "FunctionDefinition":
            # Analyze each function with isolated allocation/nullability state.
            function_scope_state = (
                self.in_function,
                self.allocations,
                self.pointer_aliases,
                self.known_container_sizes,
                self.nullable_vars,
            )
            self.in_function = True
            self.allocations = {}
            self.pointer_aliases = {}
            self.known_container_sizes = {}
            self.nullable_vars = set()

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

        if function_scope_state is not None:
            # Emit end-of-function leak diagnostics for allocations that survive.
            self._check_leaks()
            (
                self.in_function,
                self.allocations,
                self.pointer_aliases,
                self.known_container_sizes,
                self.nullable_vars,
            ) = function_scope_state

    def _check_node(self, node: ASTNode) -> None:
        node_type = type(node).__name__

        if node_type == "VariableDeclaration":
            self._check_variable_declaration(node)
        elif node_type == "FunctionCall":
            self._check_function_call(node)
        elif node_type == "DereferenceExpression":
            self._check_dereference(node)
            self._check_null_dereference(node)
        elif node_type == "IndexExpression":
            self._check_index_expression(node)
        elif node_type == "IndexAssignment":
            self._check_index_assignment(node)
        elif node_type == "BinaryOperation":
            self._check_binary_operation(node)
        elif node_type == "ReturnStatement" and self.in_function:
            self._check_leaks_on_return(node)

    def _check_variable_declaration(self, node: ASTNode) -> None:
        var_name = getattr(node, "name", None)
        value = getattr(node, "value", None)
        if not var_name or value is None:
            return

        value_type = type(value).__name__

        if value_type == "FunctionCall":
            self._check_allocation(node)
            self.pointer_aliases.pop(var_name, None)
            return

        if value_type == "AddressOfExpression":
            target_name = self._extract_identifier_name(getattr(value, "target", None))
            if target_name:
                self.pointer_aliases[var_name] = self._resolve_pointer_name(target_name)
            else:
                self.pointer_aliases.pop(var_name, None)
        elif value_type in {"Identifier", "VariableReference"}:
            source_name = self._extract_identifier_name(value)
            if source_name:
                source_root = self._resolve_pointer_name(source_name)
                if source_root in self.allocations or source_name in self.pointer_aliases:
                    self.pointer_aliases[var_name] = source_root
                else:
                    self.pointer_aliases.pop(var_name, None)
            else:
                self.pointer_aliases.pop(var_name, None)
        else:
            self.pointer_aliases.pop(var_name, None)

        # Track list literal sizes for M009 bounds checking.
        if value_type == "ListExpression":
            elements = getattr(value, "elements", []) or []
            self.known_container_sizes[var_name] = len(elements)

        # Track nullability for M010.
        if self._is_null_literal(value):
            self.nullable_vars.add(var_name)
        elif var_name in self.nullable_vars:
            self.nullable_vars.remove(var_name)

    def _check_allocation(self, node: ASTNode) -> None:
        value = getattr(node, "value", None)
        func_name = getattr(value, "name", None)
        if func_name not in {"alloc", "allocate", "malloc"}:
            return

        var_name = getattr(node, "name", None) or getattr(node, "identifier", None)
        if not var_name:
            return

        size = None
        args = getattr(value, "arguments", [])
        if args:
            size = self._extract_int(args[0])

        location = self.get_node_location(node)

        if var_name in self.allocations and not self.allocations[var_name].is_freed:
            old_alloc = self.allocations[var_name]
            self.issues.append(Issue(
                code="M001",
                severity=Severity.WARNING,
                category=Category.MEMORY,
                message=f"Memory leak: '{var_name}' reassigned without freeing previous allocation",
                location=location,
                source_line=self.get_source_line(location.line),
                suggestion=f"Free '{var_name}' before reassigning",
                fix=f"call dealloc with {var_name}\nset {var_name} to alloc with ...",
                related_locations=[old_alloc.location],
            ))

        self.allocations[var_name] = AllocationInfo(var_name=var_name, size=size, location=location)
        if size is not None:
            self.known_container_sizes[var_name] = size
        if var_name in self.nullable_vars:
            self.nullable_vars.remove(var_name)

    def _check_function_call(self, node: ASTNode) -> None:
        func_name = getattr(node, "name", None)
        if func_name in {"dealloc", "free"}:
            self._check_deallocation(node)
        elif func_name in {"realloc", "reallocate"}:
            self._check_reallocation(node)

    def _check_deallocation(self, node: ASTNode) -> None:
        args = getattr(node, "arguments", [])
        if not args:
            return

        var_name = self._extract_identifier_name(args[0])
        if not var_name:
            return

        location = self.get_node_location(node)

        root_name = self._resolve_pointer_name(var_name)

        if root_name not in self.allocations:
            self.issues.append(Issue(
                code="M002",
                severity=Severity.ERROR,
                category=Category.MEMORY,
                message=f"Attempting to free unallocated memory: '{var_name}'",
                location=location,
                source_line=self.get_source_line(location.line),
                suggestion=f"Ensure '{var_name}' is allocated before freeing",
            ))
            return

        alloc_info = self.allocations[root_name]
        if alloc_info.is_freed:
            self.issues.append(Issue(
                code="M003",
                severity=Severity.ERROR,
                category=Category.MEMORY,
                message=f"Double-free detected: '{var_name}' already freed",
                location=location,
                source_line=self.get_source_line(location.line),
                suggestion="Remove duplicate free or guard free operation",
                related_locations=[alloc_info.freed_location] if alloc_info.freed_location else [],
            ))
            return

        alloc_info.is_freed = True
        alloc_info.freed_location = location

    def _check_reallocation(self, node: ASTNode) -> None:
        args = getattr(node, "arguments", [])
        if len(args) < 2:
            return

        var_name = self._extract_identifier_name(args[0])
        if not var_name:
            return

        location = self.get_node_location(node)

        root_name = self._resolve_pointer_name(var_name)

        if root_name not in self.allocations:
            self.issues.append(Issue(
                code="M004",
                severity=Severity.WARNING,
                category=Category.MEMORY,
                message=f"Reallocating unallocated memory: '{var_name}'",
                location=location,
                source_line=self.get_source_line(location.line),
                suggestion="Use alloc/allocate for the first allocation",
            ))
            return

        alloc_info = self.allocations[root_name]
        if alloc_info.is_freed:
            self.issues.append(Issue(
                code="M005",
                severity=Severity.ERROR,
                category=Category.MEMORY,
                message=f"Reallocating freed memory: '{var_name}'",
                location=location,
                source_line=self.get_source_line(location.line),
                suggestion="Do not realloc a freed pointer",
            ))

        new_size = self._extract_int(args[1])
        if new_size is not None:
            alloc_info.size = new_size
            self.known_container_sizes[var_name] = new_size

    def _check_dereference(self, node: ASTNode) -> None:
        ptr_expr = getattr(node, "pointer", None) or getattr(node, "expression", None)
        if ptr_expr is None:
            return

        var_name = self._extract_identifier_name(ptr_expr)
        if not var_name:
            return

        var_name = self._resolve_pointer_name(var_name)

        location = self.get_node_location(node)
        alloc_info = self.allocations.get(var_name)
        if alloc_info and alloc_info.is_freed:
            self.issues.append(Issue(
                code="M006",
                severity=Severity.ERROR,
                category=Category.MEMORY,
                message=f"Use-after-free: dereferencing freed pointer '{var_name}'",
                location=location,
                source_line=self.get_source_line(location.line),
                suggestion=f"Do not dereference '{var_name}' after free",
                related_locations=[alloc_info.freed_location] if alloc_info.freed_location else [],
            ))

    def _check_null_dereference(self, node: ASTNode) -> None:
        ptr_expr = getattr(node, "pointer", None) or getattr(node, "expression", None)
        location = self.get_node_location(node)

        if self._is_null_literal(ptr_expr):
            self.issues.append(Issue(
                code="M010",
                severity=Severity.ERROR,
                category=Category.MEMORY,
                message="Null pointer dereference: attempting to dereference null literal",
                location=location,
                source_line=self.get_source_line(location.line),
                suggestion="Ensure pointer is non-null before dereferencing",
            ))
            return

        var_name = self._extract_identifier_name(ptr_expr)
        if var_name:
            var_name = self._resolve_pointer_name(var_name)
        if var_name and var_name in self.nullable_vars:
            self.issues.append(Issue(
                code="M010",
                severity=Severity.ERROR,
                category=Category.MEMORY,
                message=f"Potential null pointer dereference: '{var_name}' may be null",
                location=location,
                source_line=self.get_source_line(location.line),
                suggestion=f"Add a null check for '{var_name}' before dereference",
            ))

    def _check_index_expression(self, node: ASTNode) -> None:
        array_expr = getattr(node, "array_expr", None)
        index_expr = getattr(node, "index_expr", None)
        self._check_bounds(array_expr, index_expr, node)

    def _check_index_assignment(self, node: ASTNode) -> None:
        target = getattr(node, "target", None)
        if type(target).__name__ != "IndexExpression":
            return
        self._check_bounds(getattr(target, "array_expr", None), getattr(target, "index_expr", None), node)

    def _check_bounds(self, array_expr: Any, index_expr: Any, node: ASTNode) -> None:
        if array_expr is None or index_expr is None:
            return

        index_value = self._extract_int(index_expr)
        if index_value is None:
            return

        arr_name = self._extract_identifier_name(array_expr)
        if not arr_name:
            return

        arr_name = self._resolve_pointer_name(arr_name)

        known_size = self.known_container_sizes.get(arr_name)
        if known_size is None:
            return

        if index_value < 0 or index_value >= known_size:
            location = self.get_node_location(node)
            self.issues.append(Issue(
                code="M009",
                severity=Severity.ERROR,
                category=Category.MEMORY,
                message=(
                    f"Out-of-bounds access on '{arr_name}': index {index_value} "
                    f"is outside [0, {max(known_size - 1, 0)}]"
                ),
                location=location,
                source_line=self.get_source_line(location.line),
                suggestion=f"Guard index access with bounds check against size {known_size}",
            ))

    def _check_leaks(self) -> None:
        for var_name, alloc_info in self.allocations.items():
            if not alloc_info.is_freed:
                self.issues.append(Issue(
                    code="M007",
                    severity=Severity.WARNING,
                    category=Category.MEMORY,
                    message=f"Potential memory leak: '{var_name}' not freed",
                    location=alloc_info.location,
                    source_line=self.get_source_line(alloc_info.location.line),
                    suggestion=f"Add 'call dealloc with {var_name}' before end of scope",
                    fix=f"call dealloc with {var_name}",
                ))

    def _check_leaks_on_return(self, node: ASTNode) -> None:
        location = self.get_node_location(node)
        for var_name, alloc_info in self.allocations.items():
            if not alloc_info.is_freed:
                self.issues.append(Issue(
                    code="M008",
                    severity=Severity.WARNING,
                    category=Category.MEMORY,
                    message=f"Memory leak on return: '{var_name}' not freed before returning",
                    location=location,
                    source_line=self.get_source_line(location.line),
                    suggestion=f"Free '{var_name}' before returning",
                    related_locations=[alloc_info.location],
                ))

    def _check_binary_operation(self, node: ASTNode) -> None:
        for operand in (getattr(node, "left", None), getattr(node, "right", None)):
            name = self._extract_identifier_name(operand)
            if not name:
                continue
            root_name = self._resolve_pointer_name(name)
            alloc_info = self.allocations.get(root_name)
            if alloc_info and alloc_info.is_freed:
                location = self.get_node_location(node)
                self.issues.append(Issue(
                    code="M006",
                    severity=Severity.ERROR,
                    category=Category.MEMORY,
                    message=f"Use-after-free: expression uses freed pointer '{root_name}'",
                    location=location,
                    source_line=self.get_source_line(location.line),
                    suggestion=f"Do not use '{root_name}' after free",
                    related_locations=[alloc_info.freed_location] if alloc_info.freed_location else [],
                ))
                break

    def _resolve_pointer_name(self, name: str) -> str:
        current = name
        visited = set()
        while current in self.pointer_aliases and current not in visited:
            visited.add(current)
            current = self.pointer_aliases[current]
        return current

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

    def _extract_int(self, expr: Any) -> Optional[int]:
        if expr is None:
            return None
        expr_type = type(expr).__name__
        if expr_type in {"IntegerLiteral", "NumberLiteral"}:
            value = getattr(expr, "value", None)
            return int(value) if isinstance(value, (int, float)) else None
        if expr_type == "Literal":
            value = getattr(expr, "value", None)
            if isinstance(value, bool):
                return None
            if isinstance(value, (int, float)):
                return int(value)
        return None

    def _is_null_literal(self, expr: Any) -> bool:
        if expr is None:
            return True
        expr_type = type(expr).__name__
        if expr_type == "Literal":
            value = getattr(expr, "value", None)
            if value is None:
                return True
            if isinstance(value, str) and value.lower() in {"null", "none", "nullptr"}:
                return True
        if expr_type == "Identifier":
            name = getattr(expr, "name", "")
            return isinstance(name, str) and name.lower() in {"null", "none", "nullptr"}
        return False
