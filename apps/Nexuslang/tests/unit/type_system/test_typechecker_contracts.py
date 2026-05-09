import pytest

from nexuslang.typesystem.typechecker import TypeChecker


def test_typechecker_rejects_non_bool_enable_ownership_passes() -> None:
    with pytest.raises(TypeError, match="enable_ownership_passes must be a bool"):
        TypeChecker(enable_ownership_passes="yes")


def test_typechecker_rejects_non_bool_stop_on_ownership_errors() -> None:
    with pytest.raises(TypeError, match="stop_on_ownership_errors must be a bool"):
        TypeChecker(stop_on_ownership_errors=1)
