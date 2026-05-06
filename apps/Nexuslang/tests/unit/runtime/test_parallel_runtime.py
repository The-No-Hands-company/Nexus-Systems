"""Runtime tests for parallel for-each execution semantics."""

import os
import sys

import pytest

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "../../../src")))

from nexuslang.errors import NxlNameError, NxlRuntimeError
from tests.helpers.utils import NLPLTestBase


class TestParallelRuntime(NLPLTestBase):
    def test_parallel_for_empty_iterable_no_error(self):
        self.parse_and_execute(
            """
            set items to []
            set sentinel to 123
            parallel for each x in items
                set sentinel to 999
            end
            """
        )

        # Empty loops are a no-op.
        assert self.get_variable("sentinel") == 123

    def test_parallel_for_non_iterable_raises_runtime_error(self):
        with pytest.raises(NxlRuntimeError, match="requires an iterable collection"):
            self.parse_and_execute(
                """
                set items to 42
                parallel for each x in items
                    print text x
                end
                """
            )

    def test_parallel_for_writes_to_outer_scope_are_isolated(self):
        self.parse_and_execute(
            """
            set items to [1, 2, 3, 4]
            set total to 0
            parallel for each x in items
                set total to total plus x
            end
            """
        )

        # Each iteration runs in an isolated child interpreter scope.
        assert self.get_variable("total") == 0

    def test_parallel_for_iteration_locals_do_not_leak(self):
        with pytest.raises(NxlNameError):
            self.parse_and_execute(
                """
                set items to [10, 20]
                parallel for each x in items
                    set loop_local to x
                end
                set leaked to loop_local
                """
            )

    def test_parallel_for_iteration_error_is_wrapped(self):
        with pytest.raises(NxlRuntimeError, match="Error in parallel for each iteration"):
            self.parse_and_execute(
                """
                set items to [1, 2, 3]
                parallel for each x in items
                    set value to missing_variable
                end
                """
            )
