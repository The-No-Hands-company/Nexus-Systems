"""Regression coverage for lifetime checker try/catch block-like bodies."""

from nexuslang.parser.ast import Block, Identifier, Literal, PrintStatement, Program, TryCatchBlock
from nexuslang.typesystem.lifetime_checker import LifetimeChecker


def test_lifetime_checker_accepts_block_wrapped_trycatch_bodies() -> None:
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

    results = LifetimeChecker().check(program)

    assert results == []