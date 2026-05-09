import pytest

from nexuslang.compiler.optimizer import BoundsCheckOptimizer


def test_set_array_size_rejects_invalid_name_type() -> None:
    optimizer = BoundsCheckOptimizer()

    with pytest.raises(TypeError, match="array_name must be a non-empty string"):
        optimizer.set_array_size("", 4)



def test_set_array_size_rejects_negative_size() -> None:
    optimizer = BoundsCheckOptimizer()

    with pytest.raises(ValueError, match="size must be non-negative"):
        optimizer.set_array_size("items", -1)



def test_set_loop_bounds_rejects_invalid_range() -> None:
    optimizer = BoundsCheckOptimizer()

    with pytest.raises(ValueError, match="end must be greater than or equal to start"):
        optimizer.set_loop_bounds("i", 5, 4)



def test_mark_safe_access_rejects_invalid_name_type() -> None:
    optimizer = BoundsCheckOptimizer()

    with pytest.raises(TypeError, match="array_name must be a non-empty string"):
        optimizer.mark_safe_access("", 0)



def test_clear_loop_bounds_rejects_invalid_name_type() -> None:
    optimizer = BoundsCheckOptimizer()

    with pytest.raises(TypeError, match="loop_var must be a non-empty string"):
        optimizer.clear_loop_bounds("")
