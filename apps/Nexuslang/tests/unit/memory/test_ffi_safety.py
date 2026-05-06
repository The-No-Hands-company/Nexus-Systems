"""
Tests for FFI Safety Features:
- Unsafe FFI blocks (explicit marking with 'unsafe do ... end')
- Buffer overflow protection (_FORTIFY_SOURCE, buffer_size_annotations)
- Runtime pointer validation (ASAN / Valgrind / nxl_ffi_check_ptr)

Covers lexer, AST, parser, interpreter, CompilerOptions, and C generator.
"""

import pytest
from nexuslang.parser.lexer import Lexer, TokenType
from nexuslang.parser.ast import (
    UnsafeBlock,
    ExternFunctionDeclaration,
    VariableDeclaration,
    Literal,
    PrintStatement,
    Identifier,
)
from nexuslang.parser.parser import Parser
from nexuslang.interpreter.interpreter import Interpreter
from nexuslang.runtime.runtime import Runtime
from nexuslang.compiler import CompilerOptions
from nexuslang.compiler.backends.c_generator import CCodeGenerator


# ---------------------------------------------------------------------------
# Helper utilities
# ---------------------------------------------------------------------------

def _lex(source: str):
    lexer = Lexer(source)
    return lexer.tokenize()


def _parse(source: str):
    tokens = _lex(source)
    parser = Parser(tokens, source)
    return parser.parse()


def _run(source: str):
    """Parse and execute an NexusLang program; return interpreter instance."""
    ast = _parse(source)
    runtime = Runtime()
    interp = Interpreter(runtime)
    interp.interpret(ast)
    return interp


# ===========================================================================
# 1. LEXER: UNSAFE token
# ===========================================================================

class TestUnsafeToken:
    def test_unsafe_is_keyword(self):
        tokens = _lex("unsafe")
        types = [t.type for t in tokens if t.type != TokenType.EOF]
        assert TokenType.UNSAFE in types

    def test_unsafe_lexeme(self):
        tokens = _lex("unsafe")
        kw = next(t for t in tokens if t.type != TokenType.EOF)
        assert kw.lexeme == "unsafe"

    def test_unsafe_in_longer_program(self):
        src = "set x to 1\nunsafe do\nend"
        tokens = _lex(src)
        types = [t.type for t in tokens]
        assert TokenType.UNSAFE in types

    def test_unsafe_is_distinct_from_identifier(self):
        tokens = _lex("unsafe_mode")
        # "unsafe_mode" should be an identifier, not UNSAFE
        ident_tokens = [t for t in tokens if t.type == TokenType.IDENTIFIER]
        unsafe_tokens = [t for t in tokens if t.type == TokenType.UNSAFE]
        assert len(ident_tokens) == 1
        assert len(unsafe_tokens) == 0

    def test_unsafe_distinguishable_from_similar_words(self):
        # "safe" should not tokenise as UNSAFE
        tokens = _lex("safe")
        types = [t.type for t in tokens if t.type != TokenType.EOF]
        assert TokenType.UNSAFE not in types


# ===========================================================================
# 2. AST: UnsafeBlock node
# ===========================================================================

class TestUnsafeBlockAST:
    def test_construction_empty_body(self):
        node = UnsafeBlock(body=[], line_number=1)
        assert node.node_type == "unsafe_block"
        assert node.body == []
        assert node.line_number == 1

    def test_construction_none_body_defaults_to_empty(self):
        node = UnsafeBlock(body=None)
        assert node.body == []

    def test_construction_with_statements(self):
        inner = VariableDeclaration("x", Literal("integer", 42))
        node = UnsafeBlock(body=[inner])
        assert len(node.body) == 1
        assert node.body[0] is inner

    def test_node_type_string(self):
        node = UnsafeBlock(body=[])
        assert node.node_type == "unsafe_block"


# ===========================================================================
# 3. AST: ExternFunctionDeclaration buffer_size_annotations
# ===========================================================================

class TestExternFuncBufferAnnotations:
    def test_default_empty_list(self):
        node = ExternFunctionDeclaration("memcpy", [], None)
        assert hasattr(node, "buffer_size_annotations")
        assert node.buffer_size_annotations == []

    def test_can_set_annotations(self):
        node = ExternFunctionDeclaration("fread", [], None)
        # Annotate: param 0 (buffer) has size given by param 2 (count) * param 3 (size)
        node.buffer_size_annotations = [(0, 2)]
        assert node.buffer_size_annotations == [(0, 2)]

    def test_annotations_do_not_affect_other_fields(self):
        node = ExternFunctionDeclaration("printf", [], "Integer", "c", "cdecl", True)
        assert node.name == "printf"
        assert node.variadic is True
        assert node.buffer_size_annotations == []

    def test_independent_per_instance(self):
        a = ExternFunctionDeclaration("a", [], None)
        b = ExternFunctionDeclaration("b", [], None)
        a.buffer_size_annotations.append((0, 1))
        # b must remain unaffected
        assert b.buffer_size_annotations == []


# ===========================================================================
# 4. PARSER: parse_unsafe_block
# ===========================================================================

class TestParserUnsafeBlock:
    def test_empty_unsafe_block(self):
        ast = _parse("unsafe do\nend")
        stmts = ast.statements
        assert len(stmts) == 1
        assert isinstance(stmts[0], UnsafeBlock)
        assert stmts[0].body == []

    def test_unsafe_block_without_do(self):
        # 'do' is optional
        ast = _parse("unsafe\nset x to 1\nend")
        stmts = [s for s in ast.statements if s is not None]
        unsafe = next(s for s in stmts if isinstance(s, UnsafeBlock))
        assert isinstance(unsafe, UnsafeBlock)

    def test_unsafe_block_with_body(self):
        src = "unsafe do\nset counter to 0\nend"
        ast = _parse(src)
        unsafe_nodes = [s for s in ast.statements if isinstance(s, UnsafeBlock)]
        assert len(unsafe_nodes) == 1
        assert len(unsafe_nodes[0].body) >= 1

    def test_unsafe_block_line_number(self):
        src = "set x to 1\nunsafe do\nend"
        ast = _parse(src)
        unsafe_nodes = [s for s in ast.statements if isinstance(s, UnsafeBlock)]
        assert len(unsafe_nodes) == 1
        # Line number should be 2 (1-based)
        assert unsafe_nodes[0].line_number == 2

    def test_nested_unsafe_blocks(self):
        src = "unsafe do\nunsafe do\nend\nend"
        ast = _parse(src)
        outer = next(s for s in ast.statements if isinstance(s, UnsafeBlock))
        assert isinstance(outer, UnsafeBlock)
        inner_list = [s for s in outer.body if isinstance(s, UnsafeBlock)]
        assert len(inner_list) == 1


# ===========================================================================
# 5. INTERPRETER: unsafe context tracking
# ===========================================================================

class TestInterpreterUnsafeContext:
    def test_unsafe_context_starts_at_zero(self):
        runtime = Runtime()
        interp = Interpreter(runtime)
        assert interp._in_unsafe_context == 0

    def test_unsafe_block_increments_context(self):
        """Verify counter is > 0 while inside the block (via side-channel)."""
        runtime = Runtime()
        interp = Interpreter(runtime)

        captured = []

        # Patch execute_variable_declaration to record counter midway
        original = interp.execute_variable_declaration

        def patched(node):
            captured.append(interp._in_unsafe_context)
            return original(node)

        interp.execute_variable_declaration = patched

        src = "unsafe do\nset x to 42\nend"
        ast = _parse(src)
        interp.interpret(ast)

        assert any(v > 0 for v in captured), "Counter should be > 0 inside block"

    def test_unsafe_context_restored_after_block(self):
        interp = _run("unsafe do\nset x to 1\nend")
        assert interp._in_unsafe_context == 0

    def test_nested_unsafe_restores_correctly(self):
        interp = _run("unsafe do\nunsafe do\nset y to 2\nend\nend")
        assert interp._in_unsafe_context == 0

    def test_unsafe_block_executes_body(self):
        src = "unsafe do\nset result to 99\nend"
        interp = _run(src)
        val = interp.get_variable("result")
        assert val == 99

    def test_unsafe_block_returns_last_value(self):
        runtime = Runtime()
        interp = Interpreter(runtime)
        node = UnsafeBlock(body=[
            VariableDeclaration("z", Literal("integer", 7)),
            Literal("integer", 42),
        ])
        # Should not raise; last value returned
        interp.execute_unsafe_block(node)
        assert interp._in_unsafe_context == 0

    def test_unsafe_context_restored_on_exception(self):
        runtime = Runtime()
        interp = Interpreter(runtime)

        # Patch execute() to always raise so we confirm finally-block fires
        def always_raise(node):
            raise RuntimeError("intentional error")

        interp.execute = always_raise

        node = UnsafeBlock(body=[VariableDeclaration("x", Literal("integer", 1))])
        with pytest.raises(RuntimeError):
            interp.execute_unsafe_block(node)

        assert interp._in_unsafe_context == 0, "Counter must be restored even after exception"


# ===========================================================================
# 6. COMPILER OPTIONS: sanitizer flags
# ===========================================================================

class TestCompilerOptionsSanitizers:
    def test_sanitize_address_defaults_false(self):
        opts = CompilerOptions()
        assert opts.sanitize_address is False

    def test_sanitize_undefined_defaults_false(self):
        opts = CompilerOptions()
        assert opts.sanitize_undefined is False

    def test_enable_valgrind_defaults_false(self):
        opts = CompilerOptions()
        assert opts.enable_valgrind is False

    def test_set_sanitize_address(self):
        opts = CompilerOptions()
        opts.sanitize_address = True
        assert opts.sanitize_address is True

    def test_set_sanitize_undefined(self):
        opts = CompilerOptions()
        opts.sanitize_undefined = True
        assert opts.sanitize_undefined is True

    def test_set_enable_valgrind(self):
        opts = CompilerOptions()
        opts.enable_valgrind = True
        assert opts.enable_valgrind is True

    def test_independent_flags(self):
        opts = CompilerOptions()
        opts.sanitize_address = True
        assert opts.sanitize_undefined is False
        assert opts.enable_valgrind is False


# ===========================================================================
# 7. C GENERATOR: FFI safety macros
# ===========================================================================

class TestCGeneratorFFISafetyMacros:
    def _gen(self):
        return CCodeGenerator(target="c")

    def test_ffi_safety_macros_returns_string(self):
        gen = self._gen()
        result = gen._generate_ffi_safety_macros()
        assert isinstance(result, str)
        assert len(result) > 0

    def test_fortify_source_present(self):
        gen = self._gen()
        macros = gen._generate_ffi_safety_macros()
        assert "_FORTIFY_SOURCE" in macros

    def test_fortify_source_value_2(self):
        gen = self._gen()
        macros = gen._generate_ffi_safety_macros()
        assert "_FORTIFY_SOURCE 2" in macros

    def test_nonnull_attribute_present(self):
        gen = self._gen()
        macros = gen._generate_ffi_safety_macros()
        assert "NLPL_NONNULL" in macros

    def test_valgrind_macro_present(self):
        gen = self._gen()
        macros = gen._generate_ffi_safety_macros()
        assert "NLPL_VALGRIND_CHECK" in macros

    def test_valgrind_fallback_noop(self):
        gen = self._gen()
        macros = gen._generate_ffi_safety_macros()
        assert "((void)0)" in macros

    def test_generated_c_includes_fortify(self):
        from nexuslang.parser.ast import Program
        gen = self._gen()
        ast = Program(statements=[])
        c_code = gen.generate(ast)
        assert "_FORTIFY_SOURCE" in c_code


# ===========================================================================
# 8. C GENERATOR: nxl_ffi_check_ptr runtime function
# ===========================================================================

class TestCGeneratorFFICheckPtr:
    def _gen(self) -> CCodeGenerator:
        return CCodeGenerator(target="c")

    def test_not_emitted_by_default(self):
        gen = self._gen()
        code = gen._generate_runtime_functions()
        # Should be absent when not requested
        assert "nxl_ffi_check_ptr" not in code

    def test_emitted_when_requested(self):
        gen = self._gen()
        gen.needed_runtime_functions.add("nxl_ffi_check_ptr")
        code = gen._generate_runtime_functions()
        assert "nxl_ffi_check_ptr" in code

    def test_checks_null(self):
        gen = self._gen()
        gen.needed_runtime_functions.add("nxl_ffi_check_ptr")
        code = gen._generate_runtime_functions()
        assert "NULL" in code or "!ptr" in code

    def test_exits_on_null(self):
        gen = self._gen()
        gen.needed_runtime_functions.add("nxl_ffi_check_ptr")
        code = gen._generate_runtime_functions()
        assert "exit(1)" in code

    def test_references_valgrind_macro(self):
        gen = self._gen()
        gen.needed_runtime_functions.add("nxl_ffi_check_ptr")
        code = gen._generate_runtime_functions()
        assert "NLPL_VALGRIND_CHECK" in code


# ===========================================================================
# 9. C GENERATOR: generate_unsafe_block
# ===========================================================================

class TestCGeneratorUnsafeBlock:
    def _gen(self) -> CCodeGenerator:
        return CCodeGenerator(target="c")

    def test_unsafe_block_emits_comments(self):
        gen = self._gen()
        node = UnsafeBlock(body=[], line_number=1)
        original_buf = list(gen.output_buffer)
        gen._generate_unsafe_block(node)
        new_lines = gen.output_buffer[len(original_buf):]
        joined = "\n".join(new_lines)
        assert "unsafe block begin" in joined
        assert "unsafe block end" in joined

    def test_unsafe_block_body_is_generated(self):
        from nexuslang.parser.ast import Program
        gen = self._gen()
        ast = Program(statements=[
            UnsafeBlock(body=[
                VariableDeclaration("danger", Literal("integer", 1))
            ])
        ])
        c_code = gen.generate(ast)
        assert "danger" in c_code

    def test_empty_unsafe_block_roundtrip(self):
        from nexuslang.parser.ast import Program
        gen = self._gen()
        ast = Program(statements=[UnsafeBlock(body=[])])
        c_code = gen.generate(ast)
        assert "unsafe block begin" in c_code and "unsafe block end" in c_code


# ===========================================================================
# 10. END-TO-END: run an unsafe block via interpreter
# ===========================================================================

class TestUnsafeBlockEndToEnd:
    def test_simple_assignment_in_unsafe(self):
        src = "unsafe do\nset danger to 77\nend"
        interp = _run(src)
        assert interp.get_variable("danger") == 77

    def test_multiple_statements_in_unsafe(self):
        src = (
            "unsafe do\n"
            "set a to 10\n"
            "set b to 20\n"
            "set c to a plus b\n"
            "end"
        )
        interp = _run(src)
        assert interp.get_variable("c") == 30

    def test_unsafe_block_after_normal_code(self):
        src = (
            "set x to 5\n"
            "unsafe do\n"
            "set y to x plus 1\n"
            "end\n"
        )
        interp = _run(src)
        assert interp.get_variable("y") == 6

    def test_unsafe_block_before_normal_code(self):
        src = (
            "unsafe do\n"
            "set base to 100\n"
            "end\n"
            "set result to base plus 1\n"
        )
        interp = _run(src)
        assert interp.get_variable("result") == 101

    def test_counter_zero_after_multiple_unsafe_blocks(self):
        src = (
            "unsafe do\nset a to 1\nend\n"
            "unsafe do\nset b to 2\nend\n"
        )
        interp = _run(src)
        assert interp._in_unsafe_context == 0
