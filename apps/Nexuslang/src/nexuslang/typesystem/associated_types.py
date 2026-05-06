"""
Associated Types for the NexusLang type system.

Associated types are type members declared within traits that implementing
classes must define concretely.  They form type-level contracts alongside
method signatures.

Example (conceptual NLPL)::

    trait Iterator
        type Item                  -- associated type declaration
        function next with self returns Optional of Item
    end

    class NumberIterator implements Iterator
        type Item is Integer       -- concrete binding
        ...
    end

This module provides:
    - AssociatedTypeDecl: declaration of an associated type in a trait
    - AssociatedTypeBound: a constraint that an associated type must satisfy a trait
    - TypeProjection: the type expression T::Item (project an assoc type from T)
    - AssociatedTypeRegistry: tracks concrete bindings class -> trait -> assoc name -> type
    - GLOBAL_ASSOC_REGISTRY: module-level singleton registry
"""

from __future__ import annotations

from typing import Dict, List, Optional, Any


# ---------------------------------------------------------------------------
# AssociatedTypeDecl
# ---------------------------------------------------------------------------

class AssociatedTypeDecl:
    """Declares an associated type within a trait.

    Attributes
    ----------
    name:
        The identifier of the associated type (e.g. "Item", "Key", "Error").
    bounds:
        Zero or more trait names that the associated type must satisfy.
        These are stored as strings to avoid circular type imports.
    default:
        An optional default type used when an implementing class does not
        provide an explicit binding.  This is a Type object (or None).
    """

    def __init__(
        self,
        name: str,
        bounds: Optional[List[str]] = None,
        default: Optional[Any] = None,
    ) -> None:
        self.name = name
        self.bounds: List[str] = bounds or []
        self.default: Optional[Any] = default

    # ------------------------------------------------------------------
    # Bound validation
    # ------------------------------------------------------------------

    def has_bounds(self) -> bool:
        """True if this associated type is constrained by trait bounds."""
        return bool(self.bounds)

    def satisfies_bounds(self, concrete_type: Any, trait_lookup: Optional[Dict[str, Any]] = None) -> bool:
        """Check whether `concrete_type` satisfies all declared bounds.

        Parameters
        ----------
        concrete_type:
            The concrete Type object being bound to this associated type.
        trait_lookup:
            Optional mapping from trait name to TraitType for bound checking.
            If None, bounds are not validated (permissive mode).
        """
        if not self.bounds:
            return True
        if trait_lookup is None:
            return True  # Cannot validate without trait info

        for bound_name in self.bounds:
            trait = trait_lookup.get(bound_name)
            if trait is None:
                continue  # Unknown trait -- skip
            if hasattr(trait, "is_implemented_by") and not trait.is_implemented_by(concrete_type):
                return False
        return True

    # ------------------------------------------------------------------
    # Dunder methods
    # ------------------------------------------------------------------

    def __eq__(self, other: object) -> bool:
        return isinstance(other, AssociatedTypeDecl) and self.name == other.name

    def __hash__(self) -> int:
        return hash(("AssocTypeDecl", self.name))

    def __repr__(self) -> str:
        bounds_str = ": " + ", ".join(self.bounds) if self.bounds else ""
        default_str = f" = {self.default}" if self.default is not None else ""
        return f"type {self.name}{bounds_str}{default_str}"


# ---------------------------------------------------------------------------
# TypeProjection
# ---------------------------------------------------------------------------

class TypeProjection:
    """Represents the type expression ``T::AssocName``.

    A TypeProjection is an *abstract* type -- its concrete identity depends on
    what concrete type ``base_type`` is bound to at a given call site.

    Example::

        T = GenericParameter("T")  # T must implement Iterator
        item_type = TypeProjection(T, "Item")  # T::Item
    """

    def __init__(self, base_type: Any, associated_type_name: str) -> None:
        self.base_type = base_type
        self.associated_type_name = associated_type_name

    # ------------------------------------------------------------------
    # Resolution
    # ------------------------------------------------------------------

    def resolve(self, registry: 'AssociatedTypeRegistry', trait_name: Optional[str] = None) -> Any:
        """Attempt to resolve this projection to a concrete type.

        Looks up the concrete binding for ``base_type.name :: associated_type_name``
        in the provided registry.

        Parameters
        ----------
        registry:
            The AssociatedTypeRegistry to consult.
        trait_name:
            If provided, limit the lookup to implementations of this trait.

        Returns
        -------
        The concrete Type if found; otherwise returns ``self`` (unresolved).
        """
        base_name = getattr(self.base_type, "name", str(self.base_type))
        resolved = registry.resolve(base_name, trait_name, self.associated_type_name)
        return resolved if resolved is not None else self

    def substitute(self, substitutions: Dict[str, Any]) -> Any:
        """Replace the base_type if it appears in substitutions."""
        base_name = getattr(self.base_type, "name", None)
        if base_name and base_name in substitutions:
            new_base = substitutions[base_name]
            # Attempt to resolve with the concrete type
            new_base_name = getattr(new_base, "name", str(new_base))
            return TypeProjection(new_base, self.associated_type_name)
        return self

    # ------------------------------------------------------------------
    # Compatibility interface
    # ------------------------------------------------------------------

    def is_compatible_with(self, other: Any) -> bool:
        from nexuslang.typesystem.types import AnyType  # type: ignore[import]
        if isinstance(other, AnyType):
            return True
        if isinstance(other, TypeProjection):
            return (
                self.base_type == other.base_type
                and self.associated_type_name == other.associated_type_name
            )
        return False

    def get_common_supertype(self, other: Any) -> Any:
        from nexuslang.typesystem.types import ANY_TYPE  # type: ignore[import]
        if self == other:
            return self
        return ANY_TYPE

    def __eq__(self, other: object) -> bool:
        return (
            isinstance(other, TypeProjection)
            and self.base_type == other.base_type
            and self.associated_type_name == other.associated_type_name
        )

    def __hash__(self) -> int:
        return hash(("proj", id(self.base_type), self.associated_type_name))

    def __repr__(self) -> str:
        base_name = getattr(self.base_type, "name", str(self.base_type))
        return f"{base_name}::{self.associated_type_name}"


# ---------------------------------------------------------------------------
# AssociatedTypeRegistry
# ---------------------------------------------------------------------------

class AssociatedTypeRegistry:
    """Tracks concrete implementations of associated types.

    Internal structure::

        class_name -> trait_name -> assoc_type_name -> concrete_type

    Example::

        registry.register("NumberIterator", "Iterator", "Item", INTEGER_TYPE)
        resolved = registry.resolve("NumberIterator", "Iterator", "Item")
        # -> INTEGER_TYPE
    """

    def __init__(self) -> None:
        # { class_name: { trait_name: { assoc_name: concrete_type } } }
        self._impls: Dict[str, Dict[str, Dict[str, Any]]] = {}

    # ------------------------------------------------------------------
    # Writing
    # ------------------------------------------------------------------

    def register(
        self,
        class_name: str,
        trait_name: str,
        assoc_type_name: str,
        concrete_type: Any,
    ) -> None:
        """Bind ``concrete_type`` to the associated type ``assoc_type_name``
        as implemented by ``class_name`` for trait ``trait_name``.
        """
        if class_name not in self._impls:
            self._impls[class_name] = {}
        if trait_name not in self._impls[class_name]:
            self._impls[class_name][trait_name] = {}
        self._impls[class_name][trait_name][assoc_type_name] = concrete_type

    # ------------------------------------------------------------------
    # Reading
    # ------------------------------------------------------------------

    def resolve(
        self,
        class_name: str,
        trait_name: Optional[str],
        assoc_type_name: str,
    ) -> Optional[Any]:
        """Return the concrete type, or None if not registered.

        If ``trait_name`` is None, search all traits for the class.
        """
        class_data = self._impls.get(class_name)
        if class_data is None:
            return None

        if trait_name is not None:
            trait_data = class_data.get(trait_name)
            if trait_data is None:
                return None
            return trait_data.get(assoc_type_name)

        # Search all traits
        for trait_data in class_data.values():
            result = trait_data.get(assoc_type_name)
            if result is not None:
                return result
        return None

    def has_implementation(self, class_name: str, trait_name: str) -> bool:
        """True if the class has any associated type bindings for the trait."""
        return bool(self._impls.get(class_name, {}).get(trait_name))

    def get_all_for_class(self, class_name: str) -> Dict[str, Dict[str, Any]]:
        """Return all trait -> {assoc_name -> type} mappings for a class."""
        return dict(self._impls.get(class_name, {}))

    def get_all_for_trait(self, class_name: str, trait_name: str) -> Dict[str, Any]:
        """Return all assoc_name -> type mappings for a class/trait pair."""
        return dict(self._impls.get(class_name, {}).get(trait_name, {}))

    def class_names(self) -> List[str]:
        """Return all class names that have registered associated types."""
        return list(self._impls.keys())

    # ------------------------------------------------------------------
    # Validation
    # ------------------------------------------------------------------

    def validate_trait_implementation(
        self,
        class_name: str,
        trait_name: str,
        expected_decls: Dict[str, AssociatedTypeDecl],
        trait_lookup: Optional[Dict[str, Any]] = None,
    ) -> List[str]:
        """Validate that a class fully implements all declared associated types.

        Returns a list of error messages (empty means valid).
        """
        errors: List[str] = []
        implementations = self.get_all_for_trait(class_name, trait_name)

        for assoc_name, decl in expected_decls.items():
            if assoc_name in implementations:
                concrete = implementations[assoc_name]
                # Validate bounds
                if not decl.satisfies_bounds(concrete, trait_lookup):
                    errors.append(
                        f"Class '{class_name}': associated type '{assoc_name}' "
                        f"(bound to {concrete}) does not satisfy bounds {decl.bounds}"
                    )
            elif decl.default is not None:
                # Has a default -- not required
                pass
            else:
                errors.append(
                    f"Class '{class_name}' must provide associated type '{assoc_name}' "
                    f"for trait '{trait_name}'"
                )

        return errors

    def __repr__(self) -> str:
        count = sum(
            len(types)
            for traits in self._impls.values()
            for types in traits.values()
        )
        return f"AssociatedTypeRegistry({len(self._impls)} classes, {count} bindings)"


# ---------------------------------------------------------------------------
# Global singleton
# ---------------------------------------------------------------------------

GLOBAL_ASSOC_REGISTRY: AssociatedTypeRegistry = AssociatedTypeRegistry()


# ---------------------------------------------------------------------------
# Convenience helpers
# ---------------------------------------------------------------------------

def project(base_type: Any, assoc_name: str) -> TypeProjection:
    """Create a TypeProjection: ``base_type::assoc_name``."""
    return TypeProjection(base_type, assoc_name)


def bind_associated_type(
    class_name: str,
    trait_name: str,
    assoc_name: str,
    concrete_type: Any,
    registry: AssociatedTypeRegistry = None,
) -> None:
    """Register a concrete associated type binding in a registry.

    Uses GLOBAL_ASSOC_REGISTRY if none is provided.
    """
    reg = registry if registry is not None else GLOBAL_ASSOC_REGISTRY
    reg.register(class_name, trait_name, assoc_name, concrete_type)


def resolve_projection(
    projection: TypeProjection,
    registry: AssociatedTypeRegistry = None,
    trait_name: Optional[str] = None,
) -> Any:
    """Resolve a TypeProjection using a registry.

    Uses GLOBAL_ASSOC_REGISTRY if none is provided.
    """
    reg = registry if registry is not None else GLOBAL_ASSOC_REGISTRY
    return projection.resolve(reg, trait_name)
