"""HKT typechecker constraint hardening matrix.

This suite validates constructor/trait compatibility as a truth-table matrix
and verifies unknown traits keep deterministic error behavior.
"""

import os
import sys

import pytest


sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "..", "src"))

from nexuslang.typesystem.typechecker import TypeChecker
from nexuslang.typesystem.types import ANY_TYPE


def _checker() -> TypeChecker:
    return TypeChecker()


TRAITS = ["Functor", "Applicative", "Monad", "Foldable", "Traversable"]
CONSTRUCTORS = [
    "List",
    "Maybe",
    "Optional",
    "Result",
    "Dictionary",
    "Tree",
    "Set",
    "Integer",
]

FULLY_SUPPORTED = {"List", "Maybe", "Optional", "Result"}
PARTIAL_SUPPORTED = {"Dictionary", "Tree", "Set"}


def _expected(constructor: str, trait: str) -> bool:
    if constructor in FULLY_SUPPORTED:
        return True
    if constructor in PARTIAL_SUPPORTED:
        return trait in {"Functor", "Foldable"}
    return False


@pytest.mark.parametrize("constructor", CONSTRUCTORS)
@pytest.mark.parametrize("trait", TRAITS)
def test_hkt_constraint_truth_table_matrix(constructor: str, trait: str):
    tc = _checker()
    assert tc.check_hkt_constraint(constructor, trait) is _expected(constructor, trait)


@pytest.mark.parametrize("trait_name", ["Functor", "Monad", "Traversable"])
def test_generic_constraints_hkt_trait_names_never_report_unknown(trait_name: str):
    tc = _checker()
    tc.check_generic_constraints(
        type_params=["F"],
        type_args=[ANY_TYPE],
        constraints={"F": [trait_name]},
        context=f"matrix_{trait_name}",
    )
    unknown_trait_errors = [e for e in tc.errors if f"Unknown trait '{trait_name}'" in e]
    assert unknown_trait_errors == []


@pytest.mark.parametrize("unknown_trait", ["NotATrait", "FunctorLike", "Monoid", "Reducer"])
def test_generic_constraints_unknown_trait_errors_are_preserved(unknown_trait: str):
    tc = _checker()
    ok = tc.check_generic_constraints(
        type_params=["T"],
        type_args=[ANY_TYPE],
        constraints={"T": [unknown_trait]},
        context="unknown_trait_matrix",
    )

    assert ok is False
    assert any(f"Unknown trait '{unknown_trait}'" in err for err in tc.errors)
