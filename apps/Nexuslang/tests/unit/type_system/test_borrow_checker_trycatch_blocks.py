"""Regression coverage for borrow checker try/catch block-like bodies."""

from nexuslang.parser.ast import Block, Identifier, Literal, PrintStatement, Program, TryCatchBlock
from nexuslang.typesystem.borrow_checker import BorrowChecker


def test_borrow_checker_accepts_block_wrapped_trycatch_bodies() -> None:
    program = Program(
        [
            TryCatchBlock(
                try_block=Block([PrintStatement([Literal("string", "try")])]),
                catch_block=Block([PrintStatement([Identifier("err")])]),
                exception_var="err",
                exception_type=None,
            )
        ]
    )

    errors = BorrowChecker().check(program)

    assert errors == []