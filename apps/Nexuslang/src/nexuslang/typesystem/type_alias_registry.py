"""
Type Alias Registry for the NexusLang type system.

A type alias gives an alternative name (possibly parameterized) to an existing
type expression.  Aliases with type parameters can also carry constraints that
gate which type arguments are accepted.

Examples (conceptual NLPL)::

    type StringList is List of String
    type NumberMap is Dictionary of String and Integer
    type Pair of T is List of T          -- parameterized alias (T unconstrained)
    type SortedList of T extends Comparable is List of T   -- constrained alias

This module provides:
    - TypeAliasRegistry: stores and expands named type aliases
    - GLOBAL_ALIAS_REGISTRY: module-level singleton
    - register_alias / resolve_alias / expand_type: convenience functions
"""

from __future__ import annotations

from typing import Any, Dict, List, Optional


# ---------------------------------------------------------------------------
# TypeAliasRegistry
# ---------------------------------------------------------------------------

class TypeAliasRegistry:
    """Central registry for named type aliases with optional parameters and constraints.

    All registered aliases are ``TypeAliasType`` instances (from types.py).
    The registry handles:
        - Name-based lookup
        - Parameterized expansion via TypeAliasType.instantiate()
        - Recursive alias expansion
        - Conflict detection (re-registration raises ValueError by default)
    """

    def __init__(self) -> None:
        # alias_name -> TypeAliasType
        self._aliases: Dict[str, Any] = {}

    # ------------------------------------------------------------------
    # Registration
    # ------------------------------------------------------------------

    def register(self, alias: Any, overwrite: bool = False) -> None:
        """Register a TypeAliasType.

        Parameters
        ----------
        alias:
            A TypeAliasType instance from nexuslang.typesystem.types.
        overwrite:
            If True, silently replace an existing alias with the same name.
            If False (default), raise ValueError on duplicate registration.
        """
        name: str = alias.name
        if name in self._aliases and not overwrite:
            raise ValueError(
                f"Type alias '{name}' is already registered. "
                "Use overwrite=True to replace it."
            )
        self._aliases[name] = alias

    def register_many(self, aliases: List[Any], overwrite: bool = False) -> None:
        """Register a list of TypeAliasType instances."""
        for alias in aliases:
            self.register(alias, overwrite=overwrite)

    # ------------------------------------------------------------------
    # Lookup
    # ------------------------------------------------------------------

    def get(self, name: str) -> Optional[Any]:
        """Return the TypeAliasType for ``name``, or None if not registered."""
        return self._aliases.get(name)

    def has(self, name: str) -> bool:
        """True if an alias with this name is registered."""
        return name in self._aliases

    def all_names(self) -> List[str]:
        """Return a sorted list of all registered alias names."""
        return sorted(self._aliases.keys())

    # ------------------------------------------------------------------
    # Expansion
    # ------------------------------------------------------------------

    def expand(
        self,
        name: str,
        type_args: Optional[List[Any]] = None,
    ) -> Optional[Any]:
        """Expand a registered alias to its target type.

        For a parameterized alias, ``type_args`` must supply concrete types.
        For a simple alias, ``type_args`` should be None or empty.

        Parameters
        ----------
        name:
            The alias name to expand.
        type_args:
            Concrete type arguments (for parameterized aliases).

        Returns
        -------
        Expanded Type, or None if the alias is not registered.

        Raises
        ------
        TypeError:
            If the alias requires type arguments but none were provided, or
            if the wrong number of arguments is supplied.
        ValueError:
            If type arguments violate declared constraints.
        """
        alias = self._aliases.get(name)
        if alias is None:
            return None

        n_params = len(alias.type_parameters) if alias.type_parameters else 0

        if n_params == 0:
            # Simple alias – return the raw target type
            return alias.target_type

        if not type_args:
            raise TypeError(
                f"Type alias '{name}' requires {n_params} type argument(s), "
                "but none were provided."
            )

        if len(type_args) != n_params:
            raise TypeError(
                f"Type alias '{name}' requires {n_params} type argument(s), "
                f"but {len(type_args)} were provided."
            )

        return alias.instantiate(type_args)

    def expand_recursive(self, type_: Any, max_depth: int = 16) -> Any:
        """Recursively expand all alias references within a type.

        Traverses ListType, DictionaryType, GenericType, FunctionType, and
        UnionType, replacing any named alias references with their expansions.

        Parameters
        ----------
        type_:
            The type to expand.
        max_depth:
            Guard against infinite recursion in self-referential aliases.

        Returns
        -------
        The fully expanded type (same object if no expansion was possible).
        """
        if max_depth <= 0:
            return type_
        return self._expand_recursive_impl(type_, max_depth)

    def _expand_recursive_impl(self, type_: Any, depth: int) -> Any:
        """Internal recursive expansion."""
        # Avoid importing at module level to break circular imports
        try:
            from nexuslang.typesystem.types import (  # type: ignore[import]
                ListType, DictionaryType, GenericType, FunctionType, UnionType,
                TypeAliasType,
            )
        except ImportError:
            return type_

        if isinstance(type_, TypeAliasType):
            # Expand this alias (no params -- simple alias expansion)
            if not type_.type_parameters:
                expanded = self._expand_recursive_impl(type_.target_type, depth - 1)
                return expanded
            return type_

        if isinstance(type_, ListType):
            new_elem = self._expand_recursive_impl(type_.element_type, depth - 1)
            if new_elem is not type_.element_type:
                return ListType(new_elem)
            return type_

        if isinstance(type_, DictionaryType):
            new_key = self._expand_recursive_impl(type_.key_type, depth - 1)
            new_val = self._expand_recursive_impl(type_.value_type, depth - 1)
            if new_key is not type_.key_type or new_val is not type_.value_type:
                return DictionaryType(new_key, new_val)
            return type_

        if isinstance(type_, UnionType):
            new_types = [self._expand_recursive_impl(t, depth - 1) for t in type_.types]
            if any(a is not b for a, b in zip(new_types, type_.types)):
                result = UnionType([])
                result.types = new_types
                return result
            return type_

        if isinstance(type_, FunctionType):
            new_params = [self._expand_recursive_impl(p, depth - 1) for p in type_.param_types]
            new_ret = self._expand_recursive_impl(type_.return_type, depth - 1)
            changed = new_ret is not type_.return_type or any(
                a is not b for a, b in zip(new_params, type_.param_types)
            )
            if changed:
                return FunctionType(new_params, new_ret)
            return type_

        return type_

    # ------------------------------------------------------------------
    # Utilities
    # ------------------------------------------------------------------

    def unregister(self, name: str) -> bool:
        """Remove an alias.  Returns True if it existed."""
        if name in self._aliases:
            del self._aliases[name]
            return True
        return False

    def clear(self) -> None:
        """Remove all registered aliases."""
        self._aliases.clear()

    def clone(self) -> 'TypeAliasRegistry':
        """Return a shallow copy of this registry."""
        copy = TypeAliasRegistry()
        copy._aliases = dict(self._aliases)
        return copy

    def __len__(self) -> int:
        return len(self._aliases)

    def __contains__(self, name: str) -> bool:
        return name in self._aliases

    def __repr__(self) -> str:
        return f"TypeAliasRegistry({sorted(self._aliases.keys())})"


# ---------------------------------------------------------------------------
# Global singleton
# ---------------------------------------------------------------------------

GLOBAL_ALIAS_REGISTRY: TypeAliasRegistry = TypeAliasRegistry()


# ---------------------------------------------------------------------------
# Convenience functions
# ---------------------------------------------------------------------------

def register_alias(alias: Any, overwrite: bool = False) -> None:
    """Register a TypeAliasType in the global registry."""
    GLOBAL_ALIAS_REGISTRY.register(alias, overwrite=overwrite)


def resolve_alias(name: str, type_args: Optional[List[Any]] = None) -> Optional[Any]:
    """Look up and expand a type alias from the global registry.

    Returns None if the alias is not found.
    """
    if not GLOBAL_ALIAS_REGISTRY.has(name):
        return None
    return GLOBAL_ALIAS_REGISTRY.expand(name, type_args)


def expand_type(type_: Any, registry: TypeAliasRegistry = None) -> Any:
    """Recursively expand all alias references within a type.

    Uses GLOBAL_ALIAS_REGISTRY if none is provided.
    """
    reg = registry if registry is not None else GLOBAL_ALIAS_REGISTRY
    return reg.expand_recursive(type_)
