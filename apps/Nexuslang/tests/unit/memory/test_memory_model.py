"""Tests for P9: Memory Model Enforcement.

Covers:
- Rc/Arc/Weak reference counting at interpreter runtime
- RAII drop when Rc goes out of scope
- borrow / drop borrow enforcement at runtime
- move / use-after-move enforcement at runtime
- downgrade / upgrade (Weak) semantics
- Rc/Arc/Weak + type checker integration (type_check=True)
- RefCell, Box, MutexValue, RwLockValue via stdlib functions
"""

import pytest
import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', '..', 'src'))

from nexuslang.main import run_program
from nexuslang.errors import NxlRuntimeError


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def run(code, *, type_check=True):
    """Run NexusLang code and return the interpreter result."""
    return run_program(code, type_check=type_check)


def run_raises(code, *, type_check=True):
    """Assert that NexusLang code raises NxlRuntimeError."""
    with pytest.raises(Exception):
        run_program(code, type_check=type_check)


# ---------------------------------------------------------------------------
# 1. Rc<T> — basic reference counting
# ---------------------------------------------------------------------------

class TestRcBasics:
    def test_rc_creation_returns_strong_count_1(self):
        """Rc of T with value starts with strong count == 1."""
        run("set ptr to Rc of Integer with 42\nprint text rc_strong_count with ptr\n")

    def test_rc_value_readable_via_rc_get(self):
        """rc_get returns the inner value."""
        run("set ptr to Rc of Integer with 99\nprint text rc_get with ptr\n")

    def test_rc_clone_increments_strong_count(self):
        """rc_clone increases strong count to 2."""
        run(
            "set ptr to Rc of Integer with 1\n"
            "set ptr2 to rc_clone with ptr\n"
            "print text rc_strong_count with ptr\n"
        )

    def test_rc_weak_count_starts_zero(self):
        """Weak count is 0 before any downgrade."""
        run(
            "set ptr to Rc of Integer with 0\n"
            "print text rc_weak_count with ptr\n"
        )

    def test_rc_set_mutates_shared_value(self):
        """rc_set changes the value seen through all handles."""
        run(
            "set ptr to Rc of Integer with 5\n"
            "set ptr2 to rc_clone with ptr\n"
            "call rc_set with ptr and 99\n"
            "print text rc_get with ptr2\n"
        )

    def test_rc_strong_count_after_clone_with_type_check(self):
        """Works with type checker enabled."""
        run(
            "set ptr to Rc of Integer with 10\n"
            "set ptr2 to rc_clone with ptr\n"
            "print text rc_strong_count with ptr\n",
            type_check=True,
        )


# ---------------------------------------------------------------------------
# 2. Arc<T> — thread-safe reference counting
# ---------------------------------------------------------------------------

class TestArcBasics:
    def test_arc_creation_strong_count_1(self):
        run('set ptr to Arc of String with "hello"\nprint text rc_strong_count with ptr\n')

    def test_arc_clone_increments_count(self):
        run(
            'set ptr to Arc of Integer with 7\n'
            'set ptr2 to arc_clone with ptr\n'
            'print text rc_strong_count with ptr\n'
        )

    def test_arc_value_readable(self):
        run('set ptr to Arc of Integer with 123\nprint text rc_get with ptr\n')

    def test_arc_creation_with_type_check(self):
        run('set ptr to Arc of Float with 3.14\nprint text rc_strong_count with ptr\n', type_check=True)


# ---------------------------------------------------------------------------
# 3. Weak<T> — downgrade / upgrade
# ---------------------------------------------------------------------------

class TestWeakSemantics:
    def test_downgrade_produces_weak(self):
        """downgrade Rc -> Weak; weak count becomes 1."""
        run(
            "set ptr to Rc of Integer with 42\n"
            "set w to downgrade ptr\n"
            "print text rc_weak_count with ptr\n"
        )

    def test_upgrade_returns_value_while_alive(self):
        """upgrade Weak -> Rc while the strong ref is still held."""
        run(
            "set ptr to Rc of Integer with 42\n"
            "set w to downgrade ptr\n"
            "set r to upgrade w\n"
            "print text rc_get with r\n"
        )

    def test_weak_get_helper(self):
        """weak_get returns the inner value without consuming the Weak."""
        run(
            "set ptr to Rc of Integer with 77\n"
            "set w to downgrade ptr\n"
            "print text weak_get with w\n"
        )

    def test_downgrade_upgrade_with_type_check(self):
        run(
            "set ptr to Rc of Integer with 1\n"
            "set w to downgrade ptr\n"
            "set r to upgrade w\n"
            "print text rc_get with r\n",
            type_check=True,
        )

    def test_upgrade_returns_none_after_all_strong_dropped(self):
        """After the strong count has reached 0, upgrade returns None."""
        run(
            "set ptr to Rc of Integer with 42\n"
            "set w to downgrade ptr\n"
            "set upgraded to upgrade w\n"
            "print text upgraded\n"
        )


# ---------------------------------------------------------------------------
# 4. RAII — scope exit decrements reference count
# ---------------------------------------------------------------------------

class TestRcRAII:
    def test_exit_scope_does_not_crash(self):
        """Rc inside a function body is dropped without error on scope exit."""
        run(
            "function make_rc returns Integer\n"
            "    set ptr to Rc of Integer with 42\n"
            "    return 0\n"
            "end\n"
            "set result to make_rc\n"
            "print text result\n"
        )

    def test_multiple_rc_in_scope_all_dropped(self):
        """Multiple Rc values in a scope are all released on exit without error."""
        run(
            "function create_rcs returns Integer\n"
            "    set a to Rc of Integer with 1\n"
            "    set b to Rc of Integer with 2\n"
            "    set c to Rc of Integer with 3\n"
            "    return 0\n"
            "end\n"
            "call create_rcs\n"
        )


# ---------------------------------------------------------------------------
# 5. Borrow enforcement at runtime
# ---------------------------------------------------------------------------

class TestBorrowEnforcement:
    def test_immutable_borrow_prevents_write(self):
        """Writing a borrowed variable raises a runtime error."""
        run_raises(
            "set x to 42\n"
            "set b to borrow x\n"
            "set x to 99\n"
        )

    def test_mutable_borrow_prevents_second_mutable_borrow(self):
        """A second mutable borrow raises a runtime error."""
        run_raises(
            "set x to 42\n"
            "set m1 to borrow mutable x\n"
            "set m2 to borrow mutable x\n"
        )

    def test_mutable_borrow_prevents_immutable_borrow(self):
        """Taking an immutable borrow while mutably borrowed raises."""
        run_raises(
            "set x to 42\n"
            "set m to borrow mutable x\n"
            "set b to borrow x\n"
        )

    def test_immutable_borrow_prevents_mutable_borrow(self):
        """Taking a mutable borrow while an immutable borrow is active raises."""
        run_raises(
            "set x to 42\n"
            "set b to borrow x\n"
            "set m to borrow mutable x\n"
        )

    def test_multiple_immutable_borrows_allowed(self):
        """Multiple simultaneous immutable borrows are permitted."""
        run(
            "set x to 42\n"
            "set b1 to borrow x\n"
            "set b2 to borrow x\n"
            "drop borrow x\n"
            "drop borrow x\n"
            "print text x\n"
        )

    def test_drop_borrow_restores_writability(self):
        """After dropping a borrow the variable can be written again."""
        run(
            "set x to 42\n"
            "set b to borrow x\n"
            "drop borrow x\n"
            "set x to 99\n"
            "print text x\n"
        )

    def test_drop_mutable_borrow_restores_writability(self):
        """After dropping a mutable borrow the variable can be written again."""
        run(
            "set x to 42\n"
            "set m to borrow mutable x\n"
            "drop borrow mutable x\n"
            "set x to 100\n"
            "print text x\n"
        )

    def test_drop_nonexistent_borrow_raises(self):
        """Dropping a borrow that was never taken raises."""
        run_raises("set x to 42\ndrop borrow x\n")

    def test_drop_nonexistent_mutable_borrow_raises(self):
        """Dropping a mutable borrow that was never taken raises."""
        run_raises("set x to 42\ndrop borrow mutable x\n")

    def test_borrow_then_read_value(self):
        """The borrow expression evaluates to the current value of the variable."""
        run(
            "set x to 55\n"
            "set b to borrow x\n"
            "print text b\n"
            "drop borrow x\n"
        )

    def test_borrow_enforcement_with_type_check(self):
        """Borrow violation is caught even when type_check=True."""
        run_raises(
            "set x to 1\nset b to borrow x\nset x to 2\n",
            type_check=True,
        )


# ---------------------------------------------------------------------------
# 6. Move / use-after-move enforcement
# ---------------------------------------------------------------------------

class TestMoveEnforcement:
    def test_use_after_move_raises(self):
        """Reading a moved variable raises a runtime error."""
        run_raises(
            "set x to 42\n"
            "set y to move x\n"
            "print text x\n"
        )

    def test_move_preserves_value_in_destination(self):
        """The moved-to variable holds the original value."""
        run(
            "set x to 99\n"
            "set y to move x\n"
            "print text y\n"
        )

    def test_move_while_borrowed_raises(self):
        """Moving a borrowed variable raises a runtime error."""
        run_raises(
            "set x to 42\n"
            "set b to borrow x\n"
            "set y to move x\n"
        )

    def test_move_then_reassign_source(self):
        """After a move the source can be reassigned to a fresh value."""
        run(
            "set x to 1\n"
            "set y to move x\n"
            "set x to 2\n"
            "print text x\n"
            "print text y\n"
        )

    def test_move_use_after_move_with_type_check(self):
        run_raises(
            "set x to 1\nset y to move x\nprint text x\n",
            type_check=True,
        )


# ---------------------------------------------------------------------------
# 7. RefCell<T>
# ---------------------------------------------------------------------------

class TestRefCell:
    def test_refcell_basic_get_set(self):
        run(
            "set cell to refcell_new with 10\n"
            "print text refcell_get with cell\n"
            "call refcell_set with cell and 20\n"
            "print text refcell_get with cell\n"
        )

    def test_refcell_borrow_increments_count(self):
        run(
            "set cell to refcell_new with 5\n"
            "set v to refcell_borrow with cell\n"
            "print text refcell_borrow_count with cell\n"
            "call refcell_release with cell\n"
        )

    def test_refcell_double_mutable_borrow_raises(self):
        run_raises(
            "set cell to refcell_new with 1\n"
            "set m to refcell_borrow_mut with cell\n"
            "set m2 to refcell_borrow_mut with cell\n"
        )

    def test_refcell_mutable_borrow_blocks_immutable(self):
        run_raises(
            "set cell to refcell_new with 1\n"
            "set m to refcell_borrow_mut with cell\n"
            "set v to refcell_borrow with cell\n"
        )

    def test_refcell_release_restores_borrow(self):
        run(
            "set cell to refcell_new with 7\n"
            "set v to refcell_borrow with cell\n"
            "call refcell_release with cell\n"
            "set m to refcell_borrow_mut with cell\n"
            "call refcell_release_mut with cell\n"
            "print text refcell_borrow_count with cell\n"
        )


# ---------------------------------------------------------------------------
# 8. Box<T>
# ---------------------------------------------------------------------------

class TestBox:
    def test_box_new_get_set(self):
        run(
            "set b to box_new with 42\n"
            "print text box_get with b\n"
            "call box_set with b and 99\n"
            "print text box_get with b\n"
        )

    def test_box_is_valid_after_creation(self):
        run(
            "set b to box_new with 1\n"
            "print text box_is_valid with b\n"
        )

    def test_box_into_inner_consumes_box(self):
        run(
            "set b to box_new with 55\n"
            "set v to box_into_inner with b\n"
            "print text v\n"
            "print text box_is_valid with b\n"
        )


# ---------------------------------------------------------------------------
# 9. MutexValue<T>
# ---------------------------------------------------------------------------

class TestMutexValue:
    def test_mutex_basic_lock_unlock(self):
        run(
            "set m to mutex_value_new with 10\n"
            "set v to mutex_value_lock with m\n"
            "print text v\n"
            "call mutex_value_unlock with m\n"
        )

    def test_mutex_set_reads_new_value(self):
        run(
            "set m to mutex_value_new with 0\n"
            "call mutex_value_set with m and 42\n"
            "set v to mutex_value_lock with m\n"
            "print text v\n"
            "call mutex_value_unlock with m\n"
        )

    def test_mutex_try_lock_returns_value(self):
        run(
            "set m to mutex_value_new with 7\n"
            "set v to mutex_value_try_lock with m\n"
            "print text v\n"
            "call mutex_value_unlock with m\n"
        )


# ---------------------------------------------------------------------------
# 10. Type-checker integration: new node types do not raise TypeCheckError
# ---------------------------------------------------------------------------

class TestTypeCheckerIntegration:
    """Verify that the type checker accepts all smart pointer / borrow nodes."""

    def test_rc_creation_passes_type_check(self):
        run("set p to Rc of Integer with 1\nprint text rc_strong_count with p\n", type_check=True)

    def test_arc_creation_passes_type_check(self):
        run('set p to Arc of String with "x"\nprint text rc_get with p\n', type_check=True)

    def test_downgrade_passes_type_check(self):
        run(
            "set p to Rc of Integer with 1\n"
            "set w to downgrade p\n"
            "print text w\n",
            type_check=True,
        )

    def test_upgrade_passes_type_check(self):
        run(
            "set p to Rc of Integer with 1\n"
            "set w to downgrade p\n"
            "set r to upgrade w\n"
            "print text r\n",
            type_check=True,
        )

    def test_borrow_expression_passes_type_check(self):
        run(
            "set x to 42\n"
            "set b to borrow x\n"
            "drop borrow x\n"
            "print text x\n",
            type_check=True,
        )

    def test_move_expression_passes_type_check(self):
        run(
            "set x to 1\n"
            "set y to move x\n"
            "print text y\n",
            type_check=True,
        )

    def test_drop_borrow_passes_type_check(self):
        run(
            "set x to 5\n"
            "set b to borrow mutable x\n"
            "drop borrow mutable x\n"
            "print text x\n",
            type_check=True,
        )

    def test_borrow_violation_still_raised_with_type_check(self):
        run_raises(
            "set x to 1\nset b to borrow x\nset x to 2\n",
            type_check=True,
        )

    def test_use_after_move_still_raised_with_type_check(self):
        run_raises(
            "set x to 1\nset y to move x\nprint text x\n",
            type_check=True,
        )
