"""Tests for foreach index, null coalescing, and operator overloading.

Covers:
- for-each with index variable
- null coalescing via `otherwise`
- operator overloading in class bodies
"""

import os
import sys

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "../../../src")))

from tests.helpers.utils import NLPLTestBase


class TestForEachWithIndex(NLPLTestBase):
    def test_foreach_with_index(self):
        self.parse_and_execute(
            """
            set idx_sum to 0
            set value_sum to 0

            for each item with index i in [10, 20, 30]
                set idx_sum to idx_sum plus i
                set value_sum to value_sum plus item
            end
            """
        )

        assert self.get_variable("idx_sum") == 3
        assert self.get_variable("value_sum") == 60


class TestNullCoalescingOtherwise(NLPLTestBase):
    def test_otherwise_uses_default_for_null(self):
        self.parse_and_execute(
            """
            set x to null
            set y to x otherwise "default"
            """
        )
        assert self.get_variable("y") == "default"

    def test_otherwise_keeps_non_null_value(self):
        self.parse_and_execute(
            """
            set x to 42
            set y to x otherwise 99
            """
        )
        assert self.get_variable("y") == 42


class TestOperatorOverloading(NLPLTestBase):
    def test_operator_plus_overload(self):
        self.parse_and_execute(
            """
            class Box
                property value

                operator + with other returns Integer
                    return self.value plus other.value
                end
            end

            set left to new Box
            set left.value to 7
            set right to new Box
            set right.value to 5

            set total to left + right
            """
        )

        assert self.get_variable("total") == 12
