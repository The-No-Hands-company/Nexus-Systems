import pytest

from nexuslang.typesystem.typechecker import TypeChecker, TypeEnvironment


class _ProgramLike:
    def __init__(self, statements):
        self.statements = statements


class _DummyStatement:
    pass


def test_check_program_rejects_none_program() -> None:
    checker = TypeChecker(enable_ownership_passes=False)

    with pytest.raises(TypeError, match="program must be a Program-like node with statements"):
        checker.check_program(None)



def test_check_program_rejects_non_list_statements() -> None:
    checker = TypeChecker(enable_ownership_passes=False)

    with pytest.raises(TypeError, match="program.statements must be a list"):
        checker.check_program(_ProgramLike(statements="not-a-list"))



def test_check_statement_rejects_none_statement() -> None:
    checker = TypeChecker(enable_ownership_passes=False)

    with pytest.raises(TypeError, match="check_statement: statement must not be None"):
        checker.check_statement(None, TypeEnvironment())



def test_check_statement_rejects_invalid_env() -> None:
    checker = TypeChecker(enable_ownership_passes=False)

    with pytest.raises(TypeError, match="check_statement: env must be a TypeEnvironment"):
        checker.check_statement(_DummyStatement(), env={})



def test_check_import_statement_rejects_invalid_env() -> None:
    checker = TypeChecker(enable_ownership_passes=False)

    with pytest.raises(TypeError, match="_check_import_statement: env must be a TypeEnvironment"):
        checker._check_import_statement(_DummyStatement(), env={})



def test_check_data_structure_statement_rejects_none_statement() -> None:
    checker = TypeChecker(enable_ownership_passes=False)

    with pytest.raises(TypeError, match="_check_data_structure_statement: statement must not be None"):
        checker._check_data_structure_statement(None, TypeEnvironment())



def test_check_collection_expression_rejects_invalid_env() -> None:
    checker = TypeChecker(enable_ownership_passes=False)

    with pytest.raises(TypeError, match="_check_collection_expression: env must be a TypeEnvironment"):
        checker._check_collection_expression(_DummyStatement(), env="invalid")


def test_check_ffi_statement_rejects_non_list_parameters() -> None:
    checker = TypeChecker(enable_ownership_passes=False)

    malformed = type(
        "ExternFunctionDeclaration",
        (),
        {
            "name": "bad_extern",
            "parameters": "not-a-list",
            "return_type": "Integer",
        },
    )()

    with pytest.raises(TypeError, match="_check_ffi_statement: statement.parameters must be a list"):
        checker._check_ffi_statement(malformed, TypeEnvironment())


def test_check_inline_assembly_statement_rejects_invalid_inputs_shape() -> None:
    checker = TypeChecker(enable_ownership_passes=False)

    malformed = type(
        "InlineAssembly",
        (),
        {
            "inputs": ["bad-input-entry"],
            "outputs": [],
            "clobbers": [],
            "line_number": 1,
        },
    )()

    with pytest.raises(
        TypeError,
        match=r"each input must be a \(constraint, expression\) tuple",
    ):
        checker._check_inline_assembly_statement(malformed, TypeEnvironment())


def test_check_inline_assembly_statement_rejects_invalid_outputs_shape() -> None:
    checker = TypeChecker(enable_ownership_passes=False)

    malformed = type(
        "InlineAssembly",
        (),
        {
            "inputs": [],
            "outputs": ["bad-output-entry"],
            "clobbers": [],
            "line_number": 1,
        },
    )()

    with pytest.raises(
        TypeError,
        match=r"each output must be a \(constraint, target\) tuple",
    ):
        checker._check_inline_assembly_statement(malformed, TypeEnvironment())


def test_check_match_expression_statement_rejects_non_list_cases() -> None:
    checker = TypeChecker(enable_ownership_passes=False)

    malformed = type(
        "MatchExpression",
        (),
        {
            "expression": _DummyStatement(),
            "cases": "not-a-list",
        },
    )()

    with pytest.raises(TypeError, match="_check_match_expression_statement: statement.cases must be a list"):
        checker._check_match_expression_statement(malformed, TypeEnvironment())


def test_check_match_expression_statement_rejects_case_without_body_list() -> None:
    checker = TypeChecker(enable_ownership_passes=False)

    bad_case = type("MatchCase", (), {"pattern": _DummyStatement(), "body": "not-a-list", "guard": None})()
    malformed = type(
        "MatchExpression",
        (),
        {
            "expression": _DummyStatement(),
            "cases": [bad_case],
        },
    )()

    with pytest.raises(TypeError, match="_check_match_expression_statement: each case.body must be a list"):
        checker._check_match_expression_statement(malformed, TypeEnvironment())


def test_check_send_statement_requires_channel_attribute() -> None:
    checker = TypeChecker(enable_ownership_passes=False)
    malformed = type("SendStatement", (), {"value": _DummyStatement()})()

    with pytest.raises(TypeError, match="_check_send_statement: statement must define 'channel'"):
        checker._check_send_statement(malformed, TypeEnvironment())


def test_check_receive_expression_requires_channel_attribute() -> None:
    checker = TypeChecker(enable_ownership_passes=False)
    malformed = type("ReceiveExpression", (), {})()

    with pytest.raises(TypeError, match="_check_receive_expression: statement must define 'channel'"):
        checker._check_receive_expression(malformed, TypeEnvironment())


def test_check_close_statement_requires_channel_attribute() -> None:
    checker = TypeChecker(enable_ownership_passes=False)
    malformed = type("CloseStatement", (), {})()

    with pytest.raises(TypeError, match="_check_close_statement: statement must define 'channel'"):
        checker._check_close_statement(malformed, TypeEnvironment())


def test_refine_channel_identifier_type_rejects_invalid_payload_type() -> None:
    checker = TypeChecker(enable_ownership_passes=False)

    with pytest.raises(
        TypeError,
        match="_refine_channel_identifier_type: payload_type must be a valid NexusLang type",
    ):
        checker._refine_channel_identifier_type(_DummyStatement(), payload_type=None, env=TypeEnvironment())


def test_check_ownership_statement_requires_var_name_for_move() -> None:
    checker = TypeChecker(enable_ownership_passes=False)
    malformed = type("MoveExpression", (), {})()

    with pytest.raises(TypeError, match="_check_ownership_statement: statement must define 'var_name'"):
        checker._check_ownership_statement(malformed, TypeEnvironment())


def test_check_ownership_statement_requires_var_name_for_drop_borrow() -> None:
    checker = TypeChecker(enable_ownership_passes=False)
    malformed = type("DropBorrowStatement", (), {})()

    with pytest.raises(TypeError, match="_check_ownership_statement: statement must define 'var_name'"):
        checker._check_ownership_statement(malformed, TypeEnvironment())


def test_check_ownership_statement_requires_non_empty_lifetime_label() -> None:
    checker = TypeChecker(enable_ownership_passes=False)
    malformed = type("LifetimeAnnotation", (), {"label": ""})()

    with pytest.raises(TypeError, match="_check_ownership_statement: statement.label must be a non-empty string"):
        checker._check_ownership_statement(malformed, TypeEnvironment())


def test_check_async_function_definition_requires_name() -> None:
    checker = TypeChecker(enable_ownership_passes=False)
    malformed = type("AsyncFunctionDefinition", (), {"parameters": [], "body": []})()

    with pytest.raises(TypeError, match="check_async_function_definition: statement must define 'name'"):
        checker.check_async_function_definition(malformed, TypeEnvironment())


def test_check_async_function_definition_requires_parameters_list() -> None:
    checker = TypeChecker(enable_ownership_passes=False)
    malformed = type(
        "AsyncFunctionDefinition",
        (),
        {"name": "fetch_data", "parameters": "not-a-list", "body": []},
    )()

    with pytest.raises(TypeError, match="check_async_function_definition: statement.parameters must be a list"):
        checker.check_async_function_definition(malformed, TypeEnvironment())


def test_check_parallel_for_loop_requires_non_empty_var_name() -> None:
    checker = TypeChecker(enable_ownership_passes=False)
    malformed = type(
        "ParallelForLoop",
        (),
        {"var_name": "", "iterable": _DummyStatement(), "body": []},
    )()

    with pytest.raises(TypeError, match="check_parallel_for_loop: statement.var_name must be a non-empty string"):
        checker.check_parallel_for_loop(malformed, TypeEnvironment())


def test_check_parallel_for_loop_requires_body_list() -> None:
    checker = TypeChecker(enable_ownership_passes=False)
    malformed = type(
        "ParallelForLoop",
        (),
        {"var_name": "item", "iterable": _DummyStatement(), "body": "not-a-list"},
    )()

    with pytest.raises(TypeError, match="check_parallel_for_loop: statement.body must be a list"):
        checker.check_parallel_for_loop(malformed, TypeEnvironment())


def test_check_concurrent_block_requires_statements_list() -> None:
    checker = TypeChecker(enable_ownership_passes=False)
    malformed = type("ConcurrentBlock", (), {"statements": "not-a-list"})()

    with pytest.raises(TypeError, match="check_concurrent_block: statement.statements must be a list"):
        checker.check_concurrent_block(malformed, TypeEnvironment())


def test_check_try_catch_block_requires_try_block_shape() -> None:
    checker = TypeChecker(enable_ownership_passes=False)
    malformed = type(
        "TryCatchBlock",
        (),
        {
            "try_block": _DummyStatement(),
            "catch_block": type("Block", (), {"statements": []})(),
            "exception_var": None,
            "exception_type": None,
        },
    )()

    with pytest.raises(TypeError, match="check_try_catch_block: statement.try_block must be a Block-like node with statements"):
        checker.check_try_catch_block(malformed, TypeEnvironment())


def test_check_try_catch_block_requires_catch_block_statements_list() -> None:
    checker = TypeChecker(enable_ownership_passes=False)
    malformed = type(
        "TryCatchBlock",
        (),
        {
            "try_block": type("Block", (), {"statements": []})(),
            "catch_block": type("Block", (), {"statements": "not-a-list"})(),
            "exception_var": None,
            "exception_type": None,
        },
    )()

    with pytest.raises(TypeError, match="check_try_catch_block: statement.catch_block.statements must be a list"):
        checker.check_try_catch_block(malformed, TypeEnvironment())


def test_check_yield_expression_requires_value_attribute() -> None:
    checker = TypeChecker(enable_ownership_passes=False)
    malformed = type("YieldExpression", (), {})()

    with pytest.raises(TypeError, match="check_yield_expression: statement must define 'value'"):
        checker.check_yield_expression(malformed, TypeEnvironment())


def test_check_await_expression_requires_operand_attribute() -> None:
    checker = TypeChecker(enable_ownership_passes=False)
    malformed = type("AwaitExpression", (), {})()

    with pytest.raises(TypeError, match="check_await_expression: statement must define 'expression' or 'expr'"):
        checker.check_await_expression(malformed, TypeEnvironment())


def test_check_try_catch_requires_try_block_shape_for_alternative_form() -> None:
    checker = TypeChecker(enable_ownership_passes=False)
    malformed = type(
        "TryCatch",
        (),
        {
            "try_block": _DummyStatement(),
            "catch_block": [],
            "exception_var": None,
            "exception_type": None,
        },
    )()

    with pytest.raises(
        TypeError,
        match="check_try_catch: statement.try_block must be a list of statements or a Block-like node",
    ):
        checker.check_try_catch(malformed, TypeEnvironment())


def test_check_try_catch_requires_catch_block_statements_list_for_alternative_form() -> None:
    checker = TypeChecker(enable_ownership_passes=False)
    malformed = type(
        "TryCatch",
        (),
        {
            "try_block": [],
            "catch_block": type("Block", (), {"statements": "not-a-list"})(),
            "exception_var": None,
            "exception_type": None,
        },
    )()

    with pytest.raises(
        TypeError,
        match="check_try_catch: statement.catch_block.statements must be a list",
    ):
        checker.check_try_catch(malformed, TypeEnvironment())


def test_check_try_catch_rejects_blank_exception_var_for_alternative_form() -> None:
    checker = TypeChecker(enable_ownership_passes=False)
    malformed = type(
        "TryCatch",
        (),
        {
            "try_block": [],
            "catch_block": [],
            "exception_var": "",
            "exception_type": None,
        },
    )()

    with pytest.raises(
        TypeError,
        match="check_try_catch: statement.exception_var must be a non-empty string when provided",
    ):
        checker.check_try_catch(malformed, TypeEnvironment())


def test_check_function_call_requires_name_attribute() -> None:
    checker = TypeChecker(enable_ownership_passes=False)
    malformed = type("FunctionCall", (), {"arguments": [], "named_arguments": {}})()

    with pytest.raises(TypeError, match="check_function_call: call must define 'name'"):
        checker.check_function_call(malformed, TypeEnvironment())


def test_check_function_call_requires_non_empty_name_string() -> None:
    checker = TypeChecker(enable_ownership_passes=False)
    malformed = type("FunctionCall", (), {"name": "", "arguments": [], "named_arguments": {}})()

    with pytest.raises(TypeError, match="check_function_call: call.name must be a non-empty string"):
        checker.check_function_call(malformed, TypeEnvironment())


def test_check_function_call_requires_arguments_list() -> None:
    checker = TypeChecker(enable_ownership_passes=False)
    malformed = type("FunctionCall", (), {"name": "fn", "arguments": "not-a-list", "named_arguments": {}})()

    with pytest.raises(TypeError, match="check_function_call: call.arguments must be a list"):
        checker.check_function_call(malformed, TypeEnvironment())


def test_check_function_call_requires_named_arguments_dict() -> None:
    checker = TypeChecker(enable_ownership_passes=False)
    malformed = type("FunctionCall", (), {"name": "fn", "arguments": [], "named_arguments": ["not-a-dict"]})()

    with pytest.raises(TypeError, match="check_function_call: call.named_arguments must be a dict"):
        checker.check_function_call(malformed, TypeEnvironment())
