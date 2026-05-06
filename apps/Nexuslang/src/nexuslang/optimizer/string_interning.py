"""
String Literal Interning Optimization Pass
==========================================

Replaces duplicate string literals with references to a single interned copy,
reducing memory usage and enabling pointer-equality comparisons.

This is an NLPL-specific optimization because natural-language programs tend to
repeat string literals (English words, phrases) far more than typical code.
"""

from typing import Any, Dict, Set, List, Tuple
from . import OptimizationLevel, OptimizationPass, OptimizationStats


class StringInterningPass(OptimizationPass):
    """
    Intern duplicate string literals across the AST.

    Pass behaviour:
    - Walk the AST and collect all string literal nodes.
    - For any string that appears >= min_occurrences times, replace each
      occurrence with a reference to a shared intern-table entry.
    - The intern table is attached to the module/program root node so that
      code generation backends can emit a single string constant section.

    Attributes:
        min_occurrences: Minimum number of times a string must appear before
            it is interned.  Default 2 (intern anything that appears twice).
        _intern_table:  {original_value -> intern_name}
        _stats:         Populated after run().
    """

    # Minimum occurrences to trigger interning (avoids overhead for rarities)
    DEFAULT_MIN_OCCURRENCES = 2
    # Maximum string length to intern (very long strings are rarely duplicated)
    MAX_INTERN_LENGTH = 256

    def __init__(self, min_occurrences: int = DEFAULT_MIN_OCCURRENCES):
        super().__init__("string_interning")
        self.min_occurrences = min_occurrences
        self._intern_table: Dict[str, str] = {}  # value -> intern_variable_name
        self._string_counts: Dict[str, int] = {}
        self._strings_interned = 0
        self._nodes_replaced = 0

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------

    def should_run(self, level: OptimizationLevel) -> bool:
        """String interning starts at O1."""
        if isinstance(level, OptimizationLevel):
            return level.value >= OptimizationLevel.O1.value
        return False

    def run(self, ast: Any) -> Any:
        """
        Run string interning over the AST.

        The AST is modified in-place; the same object is returned.
        A ``__string_intern_table__`` attribute is set on the root node so
        backends can emit the string constants.
        """
        if ast is None:
            return ast

        # Phase 1 – count all string literal occurrences
        self._string_counts.clear()
        self._intern_table.clear()
        self._strings_interned = 0
        self._nodes_replaced = 0

        self._count_strings(ast)

        # Phase 2 – build intern table for frequently-used strings
        intern_counter = 0
        for value, count in self._string_counts.items():
            if count >= self.min_occurrences and len(value) <= self.MAX_INTERN_LENGTH:
                intern_name = f"__str_{intern_counter}__"
                self._intern_table[value] = intern_name
                intern_counter += 1
                self._strings_interned += 1

        if not self._intern_table:
            return ast  # Nothing to intern

        # Phase 3 – replace literal nodes with intern references
        self._replace_strings(ast)

        # Attach the intern table to the root for code-generation backends
        try:
            ast.__string_intern_table__ = self._intern_table
        except AttributeError:
            pass  # Some AST node types may not support arbitrary attributes

        self.stats.constants_folded += self._nodes_replaced
        return ast

    # ------------------------------------------------------------------
    # Private helpers
    # ------------------------------------------------------------------

    def _count_strings(self, node: Any) -> None:
        """Recursively walk AST nodes counting string literal occurrences."""
        if node is None:
            return

        node_type = type(node).__name__

        if node_type == "StringLiteral":
            value = getattr(node, "value", None)
            if isinstance(value, str):
                self._string_counts[value] = self._string_counts.get(value, 0) + 1
            return  # No children of interest in a literal

        # Recurse into all child fields
        for child in self._iter_children(node):
            self._count_strings(child)

    def _replace_strings(self, node: Any) -> None:
        """Replace internable string literals with intern-reference nodes."""
        if node is None:
            return

        node_type = type(node).__name__

        # Iterate over child fields and replace if needed
        for field_name in self._get_child_field_names(node):
            child = getattr(node, field_name, None)
            if child is None:
                continue

            if isinstance(child, list):
                new_list = []
                for item in child:
                    replaced = self._maybe_replace(item)
                    if replaced is not item:
                        self._nodes_replaced += 1
                    else:
                        self._replace_strings(item)
                    new_list.append(replaced)
                setattr(node, field_name, new_list)
            else:
                replaced = self._maybe_replace(child)
                if replaced is not child:
                    setattr(node, field_name, replaced)
                    self._nodes_replaced += 1
                else:
                    self._replace_strings(child)

    def _maybe_replace(self, node: Any) -> Any:
        """Return an InternedString node if `node` is an internable literal."""
        if node is None:
            return node
        node_type = type(node).__name__
        if node_type != "StringLiteral":
            return node
        value = getattr(node, "value", None)
        if not isinstance(value, str):
            return node
        if value not in self._intern_table:
            return node

        # Create a thin wrapper that code generators recognise
        return InternedStringRef(
            intern_name=self._intern_table[value],
            original_value=value,
            line=getattr(node, "line", 0),
        )

    def _iter_children(self, node: Any):
        """Yield all immediate child nodes (non-list and list elements)."""
        for field_name in self._get_child_field_names(node):
            child = getattr(node, field_name, None)
            if child is None:
                continue
            if isinstance(child, list):
                yield from child
            elif hasattr(child, "__dict__"):
                yield child

    def _get_child_field_names(self, node: Any) -> List[str]:
        """Return the names of AST fields that hold child nodes."""
        if not hasattr(node, "__dict__"):
            return []
        return [
            k for k, v in vars(node).items()
            if not k.startswith("_") and (
                isinstance(v, list) or hasattr(v, "__dict__")
            )
        ]

    # ------------------------------------------------------------------
    # Reporting
    # ------------------------------------------------------------------

    @property
    def strings_interned(self) -> int:
        """Number of unique strings that were interned."""
        return self._strings_interned

    @property
    def nodes_replaced(self) -> int:
        """Number of AST nodes replaced with intern references."""
        return self._nodes_replaced

    def summary(self) -> str:
        return (
            f"StringInterning: {self._strings_interned} unique strings interned, "
            f"{self._nodes_replaced} literal nodes replaced"
        )


class InternedStringRef:
    """
    AST surrogate for an interned string literal.

    Code-generation backends should emit a reference to the intern-table
    entry ``intern_name`` rather than creating a new string object.
    """

    def __init__(self, intern_name: str, original_value: str, line: int = 0):
        self.intern_name = intern_name
        self.original_value = original_value
        self.line = line
        # Keep AST compatibility: looks like a StringLiteral to generic walkers
        self.value = original_value

    def __repr__(self) -> str:
        return f"InternedStringRef({self.intern_name!r} = {self.original_value!r})"
