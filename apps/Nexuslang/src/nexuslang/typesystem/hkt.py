"""
Higher-Kinded Types (HKT) for the NexusLang type system.

Kinds are the "types of types":
    *              -- a ground type (Integer, String, List<Integer>)
    * -> *         -- a type constructor taking one type (List, Maybe)
    * -> * -> *    -- a type constructor taking two types (Either, Dictionary)
    (* -> *) -> *  -- a type constructor taking a type constructor (Functor F)

This module provides:
    - Kind hierarchy (StarKind, ArrowKind)
    - TypeConstructorParam: type variables with kind annotations
    - TypeApplication: applying a type constructor to a type argument
    - HigherKindedType: types parameterized over type constructors
    - Built-in higher-kinded traits: Functor, Monad, Foldable
"""
from __future__ import annotations

from typing import Dict, List, Optional, Any, TYPE_CHECKING, Type

if TYPE_CHECKING:
    pass


# ---------------------------------------------------------------------------
# Kind hierarchy
# ---------------------------------------------------------------------------

class Kind:
    """Base class representing the kind of a type (kind system meta-level)."""

    def arity(self) -> int:
        """Number of type arguments needed to reach kind *."""
        return 0

    def apply(self) -> Optional['Kind']:
        """Return the result kind after applying one argument, if applicable."""
        return None

    def __eq__(self, other: object) -> bool:
        if not isinstance(other, Kind):
            return NotImplemented
        return type(self) is type(other)

    def __hash__(self) -> int:
        return hash(type(self).__name__)


class StarKind(Kind):
    """Kind * -- a ground, fully-applied type (Integer, List<String>, etc.).

    StarKind is a singleton; all equality checks use identity.
    """

    _instance: Optional['StarKind'] = None

    def __new__(cls) -> 'StarKind':
        if cls._instance is None:
            cls._instance = super().__new__(cls)
        return cls._instance

    def arity(self) -> int:
        return 0

    def apply(self) -> None:
        return None

    def __repr__(self) -> str:
        return "*"

    def __eq__(self, other: object) -> bool:
        return isinstance(other, StarKind)

    def __hash__(self) -> int:
        return hash("*")


class ArrowKind(Kind):
    """Kind F -> G -- a type constructor that maps kinds.

    Examples::

        ArrowKind(STAR, STAR)             # * -> *  (List, Maybe)
        ArrowKind(STAR, STAR_TO_STAR)     # * -> (* -> *)
        ArrowKind(STAR_TO_STAR, STAR)     # (* -> *) -> *
    """

    def __init__(self, param_kind: Kind, result_kind: Kind) -> None:
        self.param_kind = param_kind
        self.result_kind = result_kind

    def arity(self) -> int:
        """Number of ground-type arguments needed."""
        return 1 + self.result_kind.arity()

    def apply(self) -> Kind:
        """Return the result kind obtained after one application."""
        return self.result_kind

    def __repr__(self) -> str:
        param_str = f"({self.param_kind})" if isinstance(self.param_kind, ArrowKind) else str(self.param_kind)
        return f"{param_str} -> {self.result_kind}"

    def __eq__(self, other: object) -> bool:
        return (
            isinstance(other, ArrowKind)
            and self.param_kind == other.param_kind
            and self.result_kind == other.result_kind
        )

    def __hash__(self) -> int:
        return hash(("->", self.param_kind, self.result_kind))


# Convenient singleton kinds
STAR: StarKind = StarKind()
STAR_TO_STAR: ArrowKind = ArrowKind(STAR, STAR)           # * -> *
STAR_TO_STAR_TO_STAR: ArrowKind = ArrowKind(STAR, STAR_TO_STAR)  # * -> (* -> *)
HO_FUNCTOR_KIND: ArrowKind = ArrowKind(STAR_TO_STAR, STAR)       # (* -> *) -> *


def kind_arity(kind: Kind) -> int:
    """Return the total arity of a kind (number of args to reach *)."""
    return kind.arity()


def kinds_equal(k1: Kind, k2: Kind) -> bool:
    """Deep equality comparison for kinds."""
    return k1 == k2


def kind_of_application(constructor_kind: Kind, arg_kind: Kind) -> Optional[Kind]:
    """Return the result kind of applying constructor_kind to arg_kind, or None on mismatch."""
    if not isinstance(constructor_kind, ArrowKind):
        return None
    if constructor_kind.param_kind != arg_kind:
        return None
    return constructor_kind.result_kind


# ---------------------------------------------------------------------------
# Type-level entities
# ---------------------------------------------------------------------------

# Deferred import to avoid circular dependency at runtime.
def _get_base_types() -> Any:
    from nexuslang.typesystem.types import (  # type: ignore[import]
        Type, AnyType, FunctionType, TraitType, ANY_TYPE,
    )
    return Type, AnyType, FunctionType, TraitType, ANY_TYPE


class TypeConstructorParam:
    """A type parameter annotated with a kind (not necessarily kind *).

    Use this when a generic requires a type constructor rather than a
    ground type::

        # Functor<F> where F :: * -> *
        F = TypeConstructorParam("F", STAR_TO_STAR)

        # Higher-order: maps a functor (kind * -> *) to a type
        G = TypeConstructorParam("G", HO_FUNCTOR_KIND)
    """

    def __init__(self, name: str, kind: Kind = None) -> None:
        self.name = name
        self.kind: Kind = kind if kind is not None else STAR

    # ------------------------------------------------------------------
    # Kind queries
    # ------------------------------------------------------------------

    def is_ground(self) -> bool:
        """True if this param is expected to be a ground type (kind *)."""
        return isinstance(self.kind, StarKind)

    def is_constructor(self) -> bool:
        """True if this param is expected to be a type constructor (kind higher than *)."""
        return isinstance(self.kind, ArrowKind)

    def expected_arity(self) -> int:
        """Number of type args needed to produce a ground type from this param."""
        return self.kind.arity()

    # ------------------------------------------------------------------
    # Compatibility (thin interface for the Type checker)
    # ------------------------------------------------------------------

    def is_compatible_with(self, other: object) -> bool:
        from nexuslang.typesystem.types import AnyType  # type: ignore[import]
        return isinstance(other, AnyType) or self == other

    def get_common_supertype(self, other: object) -> object:
        from nexuslang.typesystem.types import ANY_TYPE  # type: ignore[import]
        if self == other:
            return self
        return ANY_TYPE

    def __eq__(self, other: object) -> bool:
        return (
            isinstance(other, TypeConstructorParam)
            and self.name == other.name
            and self.kind == other.kind
        )

    def __hash__(self) -> int:
        return hash((self.name, self.kind))

    def __repr__(self) -> str:
        if self.is_ground():
            return self.name
        return f"{self.name} :: {self.kind}"


class TypeApplication:
    """The application of a type constructor to a type argument: F<A>.

    When F is a TypeConstructorParam with kind * -> *, and A has kind *,
    TypeApplication(F, A) represents F<A> which has kind *.

    Type applications can be chained::

        F<A>      -- TypeApplication(F, A)
        F<A><B>   -- TypeApplication(TypeApplication(F, A), B)  (rare)
        Dict<K,V> -- TypeApplication(TypeApplication(Dict, K), V)
    """

    def __init__(self, constructor: object, argument: object) -> None:
        self.constructor = constructor
        self.argument = argument

    # ------------------------------------------------------------------
    # Kind inference
    # ------------------------------------------------------------------

    def result_kind(self) -> Optional[Kind]:
        """Return the kind of the resulting type, or None if ill-kinded."""
        if isinstance(self.constructor, TypeConstructorParam):
            c_kind = self.constructor.kind
        else:
            # Unknown constructor; assume ground kind
            return STAR
        if isinstance(self.argument, TypeConstructorParam):
            a_kind = self.argument.kind
        else:
            a_kind = STAR
        return kind_of_application(c_kind, a_kind)

    # ------------------------------------------------------------------
    # Substitution
    # ------------------------------------------------------------------

    def substitute(self, substitutions: Dict[str, object]) -> 'TypeApplication':
        """Replace TypeConstructorParams using the given name->type map."""
        new_constructor = self.constructor
        new_argument = self.argument

        if isinstance(self.constructor, TypeConstructorParam):
            new_constructor = substitutions.get(self.constructor.name, self.constructor)
        elif isinstance(self.constructor, TypeApplication):
            new_constructor = self.constructor.substitute(substitutions)

        if isinstance(self.argument, TypeConstructorParam):
            new_argument = substitutions.get(self.argument.name, self.argument)
        elif isinstance(self.argument, TypeApplication):
            new_argument = self.argument.substitute(substitutions)

        return TypeApplication(new_constructor, new_argument)

    # ------------------------------------------------------------------
    # Compatibility interface
    # ------------------------------------------------------------------

    def is_compatible_with(self, other: object) -> bool:
        from nexuslang.typesystem.types import AnyType  # type: ignore[import]
        if isinstance(other, AnyType):
            return True
        if not isinstance(other, TypeApplication):
            return False
        # Co-variant: constructors must be equal, argument must be compatible
        return (
            self.constructor == other.constructor
            and _compat(self.argument, other.argument)
        )

    def get_common_supertype(self, other: object) -> object:
        from nexuslang.typesystem.types import ANY_TYPE  # type: ignore[import]
        if not isinstance(other, TypeApplication) or self.constructor != other.constructor:
            return ANY_TYPE
        common_arg = _common_super(self.argument, other.argument)
        if common_arg is None:
            return ANY_TYPE
        return TypeApplication(self.constructor, common_arg)

    def __eq__(self, other: object) -> bool:
        return (
            isinstance(other, TypeApplication)
            and self.constructor == other.constructor
            and self.argument == other.argument
        )

    def __hash__(self) -> int:
        return hash(("app", id(self.constructor), id(self.argument)))

    def __repr__(self) -> str:
        return f"{self.constructor}<{self.argument}>"


# ---------------------------------------------------------------------------
# HigherKindedType
# ---------------------------------------------------------------------------

class HigherKindedType:
    """A type that is parameterized over a type constructor (kind * -> *).

    This represents abstractions like Functor<F> or Traversable<T> where
    the parameter itself is a type constructor, not a ground type.

    Parameters
    ----------
    name:
        The name of the higher-kinded type (e.g. "Functor").
    constructor_params:
        Type variables that are type constructors (kind * -> * or higher).
    regular_params:
        Ordinary type variable names (kind *).
    methods:
        Dict mapping method names to their FunctionType signatures.
        Use TypeConstructorParam and TypeApplication in the signatures.
    """

    def __init__(
        self,
        name: str,
        constructor_params: List[TypeConstructorParam],
        regular_params: List[str] = None,
        methods: Dict[str, object] = None,
        parent_hkt: List['HigherKindedType'] = None,
    ) -> None:
        self.name = name
        self.constructor_params = constructor_params
        self.regular_params = regular_params or []
        self.methods = methods or {}
        self.parent_hkt = parent_hkt or []

    # ------------------------------------------------------------------
    # Instantiation
    # ------------------------------------------------------------------

    def instantiate_constructor(
        self,
        substitutions: Dict[str, object],
    ) -> Dict[str, object]:
        """Produce concrete method signatures by substituting constructor params.

        Returns a dict of method_name -> substituted_type.
        """
        result: Dict[str, object] = {}
        for method_name, method_type in self.methods.items():
            if hasattr(method_type, "substitute_params"):
                result[method_name] = method_type.substitute_params(substitutions)
            else:
                result[method_name] = method_type
        return result

    # ------------------------------------------------------------------
    # Queries
    # ------------------------------------------------------------------

    def requires_constructor_of_kind(self, kind: Kind) -> bool:
        """Check if any constructor param requires the given kind."""
        return any(p.kind == kind for p in self.constructor_params)

    def is_implemented_by_constructor(self, constructor_kind: Kind) -> bool:
        """True if a type constructor of `constructor_kind` can satisfy this HKT."""
        if not self.constructor_params:
            return True
        # All constructor_params must be satisfiable by the given constructor kind
        return all(p.kind == constructor_kind for p in self.constructor_params)

    def __repr__(self) -> str:
        params = ", ".join(
            f"{p.name} :: {p.kind}" for p in self.constructor_params
        )
        return f"HigherKinded({self.name}[{params}])"


# ---------------------------------------------------------------------------
# Helper utilities
# ---------------------------------------------------------------------------

def _compat(a: object, b: object) -> bool:
    if hasattr(a, "is_compatible_with"):
        return a.is_compatible_with(b)  # type: ignore[union-attr]
    return a == b


def _common_super(a: object, b: object) -> Optional[object]:
    if hasattr(a, "get_common_supertype"):
        return a.get_common_supertype(b)  # type: ignore[union-attr]
    from nexuslang.typesystem.types import ANY_TYPE  # type: ignore[import]
    return ANY_TYPE if a != b else a


# ---------------------------------------------------------------------------
# Built-in higher-kinded traits
# ---------------------------------------------------------------------------

def _make_builtin_hkt_traits() -> Dict[str, HigherKindedType]:
    """Construct Functor, Monad, Foldable, Applicative as HigherKindedType instances."""

    F = TypeConstructorParam("F", STAR_TO_STAR)
    A = TypeConstructorParam("A", STAR)
    B = TypeConstructorParam("B", STAR)

    # Functor: map :: (A -> B) -> F<A> -> F<B>
    functor = HigherKindedType(
        name="Functor",
        constructor_params=[F],
        regular_params=["A", "B"],
        methods={
            "map": {
                "params": [
                    TypeApplication(F, A),          # F<A>
                    "A -> B",                        # function A -> B (simplified)
                ],
                "return": TypeApplication(F, B),    # F<B>
            }
        },
    )

    # Applicative (extends Functor): pure :: A -> F<A>, ap :: F<A -> B> -> F<A> -> F<B>
    applicative = HigherKindedType(
        name="Applicative",
        constructor_params=[F],
        regular_params=["A", "B"],
        parent_hkt=[functor],
        methods={
            "pure": {
                "params": [A],
                "return": TypeApplication(F, A),
            },
            "ap": {
                "params": [
                    TypeApplication(F, "A -> B"),   # F<A -> B>
                    TypeApplication(F, A),           # F<A>
                ],
                "return": TypeApplication(F, B),    # F<B>
            },
            "map": {
                "params": [TypeApplication(F, A), "A -> B"],
                "return": TypeApplication(F, B),
            },
        },
    )

    # Monad (extends Applicative): bind :: F<A> -> (A -> F<B>) -> F<B>
    monad = HigherKindedType(
        name="Monad",
        constructor_params=[F],
        regular_params=["A", "B"],
        parent_hkt=[applicative, functor],
        methods={
            "unit": {
                "params": [A],
                "return": TypeApplication(F, A),
            },
            "bind": {
                "params": [
                    TypeApplication(F, A),           # F<A>
                    "A -> F<B>",                     # continuation (simplified)
                ],
                "return": TypeApplication(F, B),    # F<B>
            },
            "map": {
                "params": [TypeApplication(F, A), "A -> B"],
                "return": TypeApplication(F, B),
            },
        },
    )

    # Foldable: fold :: F<A> -> B -> (B -> A -> B) -> B
    foldable = HigherKindedType(
        name="Foldable",
        constructor_params=[F],
        regular_params=["A", "B"],
        methods={
            "fold": {
                "params": [
                    TypeApplication(F, A),           # F<A>
                    B,                               # initial value
                    "B -> A -> B",                   # accumulation function
                ],
                "return": B,
            },
            "to_list": {
                "params": [TypeApplication(F, A)],
                "return": "List<A>",
            },
        },
    )

    # Traversable (extends Functor and Foldable)
    traversable = HigherKindedType(
        name="Traversable",
        constructor_params=[F],
        regular_params=["A", "B"],
        parent_hkt=[functor, foldable],
        methods={
            "traverse": {
                "params": [
                    TypeApplication(F, A),
                    "A -> G<B>",               # where G is Applicative
                ],
                "return": "G<F<B>>",           # simplified
            },
            "sequence": {
                "params": [TypeApplication(F, TypeApplication(F, A))],
                "return": TypeApplication(F, TypeApplication(F, A)),
            },
        },
    )

    return {
        "Functor": functor,
        "Applicative": applicative,
        "Monad": monad,
        "Foldable": foldable,
        "Traversable": traversable,
    }


# Module-level singleton registry of built-in HKT traits
HKT_TRAITS: Dict[str, HigherKindedType] = _make_builtin_hkt_traits()

FUNCTOR_HKT: HigherKindedType = HKT_TRAITS["Functor"]
APPLICATIVE_HKT: HigherKindedType = HKT_TRAITS["Applicative"]
MONAD_HKT: HigherKindedType = HKT_TRAITS["Monad"]
FOLDABLE_HKT: HigherKindedType = HKT_TRAITS["Foldable"]
TRAVERSABLE_HKT: HigherKindedType = HKT_TRAITS["Traversable"]


# ---------------------------------------------------------------------------
# User-facing HKT registry
# ---------------------------------------------------------------------------

class HKTRegistry:
    """Registry for user-defined higher-kinded types and traits.

    Maintains a mapping from HKT name to HigherKindedType definition.
    Also tracks which concrete type constructors satisfy which HKTs.
    """

    def __init__(self) -> None:
        self._hkts: Dict[str, HigherKindedType] = {}
        # constructor_name -> list of satisfied HKT names
        self._implementations: Dict[str, List[str]] = {}

    def register(self, hkt: HigherKindedType) -> None:
        """Register a new higher-kinded type."""
        self._hkts[hkt.name] = hkt

    def get(self, name: str) -> Optional[HigherKindedType]:
        """Look up an HKT by name."""
        return self._hkts.get(name)

    def register_implementation(self, constructor_name: str, hkt_name: str) -> None:
        """Declare that a type constructor satisfies a given HKT."""
        if constructor_name not in self._implementations:
            self._implementations[constructor_name] = []
        if hkt_name not in self._implementations[constructor_name]:
            self._implementations[constructor_name].append(hkt_name)

    def implements(self, constructor_name: str, hkt_name: str) -> bool:
        """Check if a type constructor satisfies a given HKT."""
        return hkt_name in self._implementations.get(constructor_name, [])

    def get_implementations(self, constructor_name: str) -> List[str]:
        """Return all HKT names satisfied by a constructor."""
        return list(self._implementations.get(constructor_name, []))

    def all_names(self) -> List[str]:
        """Return all registered HKT names."""
        return list(self._hkts.keys())

    def __repr__(self) -> str:
        return f"HKTRegistry({list(self._hkts.keys())})"


# Global registry pre-populated with built-in HKTs
GLOBAL_HKT_REGISTRY: HKTRegistry = HKTRegistry()
for _hkt in HKT_TRAITS.values():
    GLOBAL_HKT_REGISTRY.register(_hkt)

# Common type constructors implement Functor, Applicative, Monad
for _constructor in ("List", "Maybe", "Optional", "Result"):
    GLOBAL_HKT_REGISTRY.register_implementation(_constructor, "Functor")
    GLOBAL_HKT_REGISTRY.register_implementation(_constructor, "Applicative")
    GLOBAL_HKT_REGISTRY.register_implementation(_constructor, "Monad")
    GLOBAL_HKT_REGISTRY.register_implementation(_constructor, "Foldable")
    GLOBAL_HKT_REGISTRY.register_implementation(_constructor, "Traversable")

for _constructor in ("Dictionary", "Tree", "Set"):
    GLOBAL_HKT_REGISTRY.register_implementation(_constructor, "Functor")
    GLOBAL_HKT_REGISTRY.register_implementation(_constructor, "Foldable")
