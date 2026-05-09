"""
LLVM IR and C codegen tests for contract/assertion lowering.

Covers require, ensure, guarantee, invariant, and expect statements,
verifying that each produces a conditional branch to a panic/abort path
with the correct failure message.
"""

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', '..', 'src'))

from nexuslang.compiler.backends.llvm_ir_generator import LLVMIRGenerator
from nexuslang.compiler.backends.c_generator import CCodeGenerator
from nexuslang.parser.ast import (
    Program,
    FunctionDefinition,
    Parameter,
    FunctionCall,
    ReturnStatement,
    RequireStatement,
    EnsureStatement,
    GuaranteeStatement,
    InvariantStatement,
    ExpectStatement,
    Literal,
    Identifier,
    VariableDeclaration,
    BinaryOperation,
    AsyncFunctionDefinition,
    AwaitExpression,
    YieldExpression,
    MoveExpression,
    BorrowExpression,
    BorrowExpressionWithLifetime,
    DropBorrowStatement,
)


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _llvm(nodes):
    return LLVMIRGenerator().generate(Program(nodes))


def _c(nodes):
    return CCodeGenerator(target="c").generate(Program(nodes))


# ---------------------------------------------------------------------------
# LLVM: require
# ---------------------------------------------------------------------------

class TestLLVMRequireStatement:
    def test_require_true_literal_emits_branch(self):
        ir = _llvm([RequireStatement(condition=Literal("boolean", True))])
        assert "br i1" in ir

    def test_require_false_literal_emits_nxl_panic(self):
        ir = _llvm([RequireStatement(condition=Literal("boolean", False))])
        assert "nxl_panic" in ir

    def test_require_emits_contract_ok_and_fail_labels(self):
        ir = _llvm([RequireStatement(condition=Literal("boolean", True))])
        assert "contract.ok" in ir
        assert "contract.fail" in ir

    def test_require_default_message_contains_require(self):
        ir = _llvm([RequireStatement(condition=Literal("boolean", False))])
        assert "require" in ir.lower()

    def test_require_custom_message_included(self):
        ir = _llvm([
            RequireStatement(
                condition=Literal("boolean", False),
                message_expr=Literal("string", "x must be positive"),
            )
        ])
        assert "x must be positive" in ir

    def test_require_with_identifier_condition(self):
        ir = _llvm([
            VariableDeclaration("flag", Literal("boolean", True)),
            RequireStatement(condition=Identifier("flag")),
        ])
        assert "contract.ok" in ir
        assert "contract.fail" in ir

    def test_require_inside_function(self):
        ir = _llvm([
            FunctionDefinition(
                name="validated",
                parameters=[],
                body=[RequireStatement(condition=Literal("boolean", True))],
            )
        ])
        assert "define" in ir
        assert "contract.ok" in ir


# ---------------------------------------------------------------------------
# LLVM: ensure
# ---------------------------------------------------------------------------

class TestLLVMEnsureStatement:
    def test_ensure_emits_contract_labels(self):
        ir = _llvm([EnsureStatement(condition=Literal("boolean", True))])
        assert "contract.ok" in ir
        assert "contract.fail" in ir

    def test_ensure_default_message_contains_ensure(self):
        ir = _llvm([EnsureStatement(condition=Literal("boolean", False))])
        assert "ensure" in ir.lower()

    def test_ensure_custom_message_included(self):
        ir = _llvm([
            EnsureStatement(
                condition=Literal("boolean", False),
                message_expr=Literal("string", "result must be valid"),
            )
        ])
        assert "result must be valid" in ir


# ---------------------------------------------------------------------------
# LLVM: guarantee
# ---------------------------------------------------------------------------

class TestLLVMGuaranteeStatement:
    def test_guarantee_emits_branch(self):
        ir = _llvm([GuaranteeStatement(condition=Literal("boolean", True))])
        assert "br i1" in ir

    def test_guarantee_default_message_contains_guarantee(self):
        ir = _llvm([GuaranteeStatement(condition=Literal("boolean", False))])
        assert "guarantee" in ir.lower()


# ---------------------------------------------------------------------------
# LLVM: invariant
# ---------------------------------------------------------------------------

class TestLLVMInvariantStatement:
    def test_invariant_emits_branch(self):
        ir = _llvm([InvariantStatement(condition=Literal("boolean", True))])
        assert "br i1" in ir

    def test_invariant_default_message_contains_invariant(self):
        ir = _llvm([InvariantStatement(condition=Literal("boolean", False))])
        assert "invariant" in ir.lower()


# ---------------------------------------------------------------------------
# LLVM: expect
# ---------------------------------------------------------------------------

class TestLLVMExpectStatement:
    def test_expect_equal_integers_emits_icmp(self):
        ir = _llvm([
            ExpectStatement(
                actual_expr=Literal("integer", 5),
                expected_expr=Literal("integer", 5),
                matcher="equal",
            )
        ])
        assert "icmp eq" in ir
        assert "contract.ok" in ir

    def test_expect_greater_than_emits_icmp_sgt(self):
        ir = _llvm([
            ExpectStatement(
                actual_expr=Literal("integer", 10),
                expected_expr=Literal("integer", 3),
                matcher="greater_than",
            )
        ])
        assert "icmp sgt" in ir

    def test_expect_less_than_emits_icmp_slt(self):
        ir = _llvm([
            ExpectStatement(
                actual_expr=Literal("integer", 1),
                expected_expr=Literal("integer", 9),
                matcher="less_than",
            )
        ])
        assert "icmp slt" in ir

    def test_expect_be_true_emits_branch(self):
        ir = _llvm([
            ExpectStatement(
                actual_expr=Literal("boolean", True),
                matcher="be_true",
            )
        ])
        assert "br i1" in ir

    def test_expect_be_false_emits_xor_and_branch(self):
        ir = _llvm([
            ExpectStatement(
                actual_expr=Literal("boolean", False),
                matcher="be_false",
            )
        ])
        assert "xor i1" in ir

    def test_expect_negated_flips_condition(self):
        ir_normal = _llvm([
            ExpectStatement(
                actual_expr=Literal("integer", 5),
                expected_expr=Literal("integer", 5),
                matcher="equal",
                negated=False,
            )
        ])
        ir_negated = _llvm([
            ExpectStatement(
                actual_expr=Literal("integer", 5),
                expected_expr=Literal("integer", 5),
                matcher="equal",
                negated=True,
            )
        ])
        # Negated form must introduce an extra xor to flip the condition.
        assert ir_negated.count("xor i1") > ir_normal.count("xor i1")


# ---------------------------------------------------------------------------
# C backend: require / ensure / guarantee / invariant
# ---------------------------------------------------------------------------

class TestCContractStatements:
    def test_c_require_emits_if_abort_pattern(self):
        c = _c([RequireStatement(condition=Literal("boolean", True))])
        assert "if (!(" in c
        assert "exit(1)" in c

    def test_c_require_default_message(self):
        c = _c([RequireStatement(condition=Literal("boolean", False))])
        assert "require" in c.lower()

    def test_c_ensure_emits_guard(self):
        c = _c([EnsureStatement(condition=Literal("boolean", True))])
        assert "if (!(" in c
        assert "exit(1)" in c

    def test_c_ensure_default_message(self):
        c = _c([EnsureStatement(condition=Literal("boolean", False))])
        assert "ensure" in c.lower()

    def test_c_guarantee_emits_guard(self):
        c = _c([GuaranteeStatement(condition=Literal("boolean", True))])
        assert "if (!(" in c
        assert "exit(1)" in c

    def test_c_guarantee_default_message(self):
        c = _c([GuaranteeStatement(condition=Literal("boolean", False))])
        assert "guarantee" in c.lower()

    def test_c_invariant_emits_guard(self):
        c = _c([InvariantStatement(condition=Literal("boolean", True))])
        assert "if (!(" in c
        assert "exit(1)" in c

    def test_c_invariant_default_message(self):
        c = _c([InvariantStatement(condition=Literal("boolean", False))])
        assert "invariant" in c.lower()

    def test_c_contract_custom_message_emitted(self):
        c = _c([
            RequireStatement(
                condition=Literal("boolean", False),
                message_expr=Literal("string", "value must be non-negative"),
            )
        ])
        assert "value must be non-negative" in c

    def test_c_stderr_fprintf_used_for_failure_message(self):
        c = _c([RequireStatement(condition=Literal("boolean", False))])
        assert "fprintf" in c
        assert "stderr" in c


# ---------------------------------------------------------------------------
# C backend: expect
# ---------------------------------------------------------------------------

class TestCExpectStatement:
    def test_c_expect_equal_emits_equality_check(self):
        c = _c([
            ExpectStatement(
                actual_expr=Literal("integer", 5),
                expected_expr=Literal("integer", 5),
                matcher="equal",
            )
        ])
        assert "==" in c
        assert "exit(1)" in c

    def test_c_expect_greater_than_emits_gt_check(self):
        c = _c([
            ExpectStatement(
                actual_expr=Literal("integer", 10),
                expected_expr=Literal("integer", 3),
                matcher="greater_than",
            )
        ])
        assert ">" in c

    def test_c_expect_less_than_emits_lt_check(self):
        c = _c([
            ExpectStatement(
                actual_expr=Literal("integer", 1),
                expected_expr=Literal("integer", 9),
                matcher="less_than",
            )
        ])
        assert "<" in c

    def test_c_expect_be_true_emits_truthy_check(self):
        c = _c([
            ExpectStatement(
                actual_expr=Literal("boolean", True),
                matcher="be_true",
            )
        ])
        assert "if (!(" in c

    def test_c_expect_be_false_emits_negated_check(self):
        c = _c([
            ExpectStatement(
                actual_expr=Literal("boolean", True),
                matcher="be_false",
            )
        ])
        assert "!(" in c


# ---------------------------------------------------------------------------
# Generic specialization parity: LLVM and C backends
# ---------------------------------------------------------------------------

class TestGenericSpecializationParity:
    def _generic_identity_program(self, call_expr):
        generic_identity = FunctionDefinition(
            name="identity",
            parameters=[Parameter("value", "T")],
            body=[ReturnStatement(Identifier("value"))],
            return_type="T",
            type_parameters=["T"],
        )
        return Program([
            generic_identity,
            VariableDeclaration("out", call_expr),
        ])

    def test_llvm_explicit_type_arguments_emit_specialized_function(self):
        program = self._generic_identity_program(
            FunctionCall("identity", [Literal("integer", 42)], type_arguments=["Integer"])
        )
        ir = LLVMIRGenerator().generate(program)
        assert "define i64 @identity_Integer" in ir
        assert "call i64 @identity_Integer(i64 42)" in ir

    def test_c_explicit_type_arguments_emit_specialized_function(self):
        program = self._generic_identity_program(
            FunctionCall("identity", [Literal("integer", 42)], type_arguments=["Integer"])
        )
        c = CCodeGenerator(target="c").generate(program)
        assert "int identity_Integer(int value)" in c
        assert "identity_Integer(42)" in c

    def test_llvm_inferred_type_arguments_emit_specialized_function(self):
        program = self._generic_identity_program(
            FunctionCall("identity", [Literal("string", "hello")])
        )
        ir = LLVMIRGenerator().generate(program)
        assert "@identity_String" in ir

    def test_c_inferred_type_arguments_emit_specialized_function(self):
        program = self._generic_identity_program(
            FunctionCall("identity", [Literal("string", "hello")])
        )
        c = CCodeGenerator(target="c").generate(program)
        assert "const char* identity_String(const char* value)" in c
        assert "identity_String(\"hello\")" in c


# ---------------------------------------------------------------------------
# Ownership lowering parity: LLVM and C backends
# ---------------------------------------------------------------------------

class TestOwnershipLoweringParity:
    def test_llvm_move_expression_loads_then_invalidates_source(self):
        ir = _llvm([
            FunctionDefinition(
                name="consume",
                parameters=[],
                body=[
                    VariableDeclaration("value", Literal("integer", 41)),
                    VariableDeclaration("moved", MoveExpression("value")),
                    ReturnStatement(Identifier("moved")),
                ],
                return_type="Integer",
            )
        ])
        assert "store i64 41" in ir
        assert "store i64 0" in ir

    def test_llvm_borrow_expression_with_lifetime_reads_source_value(self):
        ir = _llvm([
            FunctionDefinition(
                name="borrow_value",
                parameters=[],
                body=[
                    VariableDeclaration("value", Literal("integer", 7)),
                    VariableDeclaration(
                        "borrowed",
                        BorrowExpressionWithLifetime("value", mutable=False),
                    ),
                    ReturnStatement(Identifier("borrowed")),
                ],
                return_type="Integer",
            )
        ])
        assert "load i64" in ir

    def test_llvm_drop_borrow_statement_is_supported(self):
        ir = _llvm([
            FunctionDefinition(
                name="drop_demo",
                parameters=[],
                body=[
                    VariableDeclaration("value", Literal("integer", 5)),
                    VariableDeclaration("ref", BorrowExpression("value", mutable=False)),
                    DropBorrowStatement("value", mutable=False),
                    ReturnStatement(Identifier("ref")),
                ],
                return_type="Integer",
            )
        ])
        assert "Unhandled" not in ir

    def test_c_move_expression_invalidates_source_binding(self):
        c = _c([
            FunctionDefinition(
                name="consume",
                parameters=[],
                body=[
                    VariableDeclaration("value", Literal("integer", 41)),
                    VariableDeclaration("moved", MoveExpression("value")),
                    ReturnStatement(Identifier("moved")),
                ],
                return_type="Integer",
            )
        ])
        assert "value = 0;" in c
        assert "_nxl_moved" in c

    def test_c_borrow_and_drop_borrow_are_lowered(self):
        c = _c([
            FunctionDefinition(
                name="borrow_value",
                parameters=[],
                body=[
                    VariableDeclaration("value", Literal("integer", 7)),
                    VariableDeclaration("borrowed", BorrowExpression("value", mutable=False)),
                    DropBorrowStatement("value", mutable=False),
                    ReturnStatement(Identifier("borrowed")),
                ],
                return_type="Integer",
            )
        ])
        assert "int borrowed = value;" in c
        assert "Unhandled" not in c


# ---------------------------------------------------------------------------
# Async lowering parity: LLVM and C backends
# ---------------------------------------------------------------------------

class TestAsyncLoweringParity:
    def test_llvm_async_function_uses_coroutine_signature(self):
        ir = _llvm([
            AsyncFunctionDefinition(
                name="fetch",
                parameters=[],
                body=[ReturnStatement(Literal("integer", 7))],
                return_type="Integer",
            )
        ])
        assert "presplitcoroutine" in ir
        assert "define i8* @fetch()" in ir

    def test_c_async_function_uses_coroutine_frame_signature(self):
        c = _c([
            AsyncFunctionDefinition(
                name="fetch",
                parameters=[],
                body=[ReturnStatement(Literal("integer", 7))],
                return_type="Integer",
            )
        ])
        assert "NXLAsyncFrame* fetch(void)" in c
        assert "nxl_async_frame_create" in c

    def test_await_async_function_emits_polling_path_in_both_backends(self):
        program = [
            AsyncFunctionDefinition(
                name="fetch",
                parameters=[],
                body=[ReturnStatement(Literal("integer", 7))],
                return_type="Integer",
            ),
            AsyncFunctionDefinition(
                name="wrapper",
                parameters=[],
                body=[
                    VariableDeclaration("value", AwaitExpression(Identifier("fetch"))),
                    ReturnStatement(Identifier("value")),
                ],
                return_type="Integer",
            ),
        ]

        ir = _llvm(program)
        c = _c(program)

        assert "await.poll." in ir
        assert "if (_nxl_task) { while (!_nxl_task->done)" in c


# ---------------------------------------------------------------------------
# Generator frame lowering depth: LLVM backend
# ---------------------------------------------------------------------------

class TestGeneratorFrameLowering:
    def test_llvm_yield_function_emits_state_machine_globals_and_labels(self):
        ir = _llvm([
            FunctionDefinition(
                name="generate_values",
                parameters=[Parameter("start", "Integer")],
                body=[
                    VariableDeclaration("current", Identifier("start")),
                    YieldExpression(Identifier("current")),
                    VariableDeclaration("current", BinaryOperation(Identifier("current"), "+", Literal("integer", 1))),
                    YieldExpression(Identifier("current")),
                    ReturnStatement(Identifier("current")),
                ],
                return_type="Integer",
            )
        ])

        assert "@.yield_state.generate_values" in ir
        assert "@.yield_param.generate_values.start" in ir
        assert "@.yield_spill.generate_values.current" in ir
        assert "switch i64" in ir
        assert "yield.state." in ir

    def test_llvm_multiple_yield_functions_get_isolated_state_globals(self):
        ir = _llvm([
            FunctionDefinition(
                name="gen_a",
                parameters=[],
                body=[YieldExpression(Literal("integer", 1))],
                return_type="Integer",
            ),
            FunctionDefinition(
                name="gen_b",
                parameters=[],
                body=[YieldExpression(Literal("integer", 10))],
                return_type="Integer",
            ),
        ])

        assert "@.yield_state.gen_a" in ir
        assert "@.yield_state.gen_b" in ir
