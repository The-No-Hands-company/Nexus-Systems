"""C backend channel lowering coverage."""

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', '..', 'src'))

from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser
from nexuslang.compiler.backends.c_generator import CCodeGenerator


def _parse(code: str):
    lexer = Lexer(code)
    tokens = lexer.scan_tokens()
    parser = Parser(tokens)
    return parser.parse()


def test_c_codegen_includes_channel_runtime_and_calls():
    code = """
    set ch to create channel
    send 1 to ch
    set x to receive from ch
    """

    ast = _parse(code)
    generator = CCodeGenerator(target="c")
    c_code = generator.generate(ast)

    assert "nxl_channel_create" in c_code
    assert "nxl_channel_send" in c_code
    assert "nxl_channel_receive" in c_code
    assert "typedef struct NLPLChannel" in c_code


def test_c_codegen_uses_intptr_t_channel_payload_transport():
    code = """
    set ch to create channel
    send 42 to ch
    set x to receive from ch
    """

    ast = _parse(code)
    generator = CCodeGenerator(target="c")
    c_code = generator.generate(ast)

    assert "(intptr_t)(42)" in c_code
    assert "intptr_t nxl_channel_receive(void* channel)" in c_code
    assert "while (!ch->head && !ch->closed)" in c_code


def test_c_codegen_channel_runtime_is_thread_safe_and_close_aware():
    code = """
    set ch to create channel
    send 1 to ch
    set x to receive from ch
    """

    ast = _parse(code)
    generator = CCodeGenerator(target="c")
    c_code = generator.generate(ast)

    assert "#include <pthread.h>" in c_code
    assert "pthread_mutex_t lock;" in c_code
    assert "pthread_cond_t has_data;" in c_code
    assert "void nxl_channel_close(void* channel)" in c_code
    assert "pthread_cond_wait(&ch->has_data, &ch->lock);" in c_code
    assert "pthread_cond_signal(&ch->has_data);" in c_code
    assert "if (!ch->head && ch->closed)" in c_code


def test_c_codegen_lowers_close_statement_to_runtime_call():
    code = """
    set ch to create channel
    close ch
    """

    ast = _parse(code)
    generator = CCodeGenerator(target="c")
    c_code = generator.generate(ast)

    assert "void nxl_channel_close(void* channel)" in c_code
    assert "nxl_channel_close(ch);" in c_code
