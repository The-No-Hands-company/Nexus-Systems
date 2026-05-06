"""LLVM backend channel runtime lowering coverage."""

import os
import sys


sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', '..', 'src'))

from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser
from nexuslang.compiler.backends.llvm_ir_generator import LLVMIRGenerator


def _parse(code: str):
    lexer = Lexer(code)
    tokens = lexer.scan_tokens()
    parser = Parser(tokens)
    return parser.parse()


def test_llvm_generates_channel_runtime_calls():
    code = """
    set ch to create channel
    send 1 to ch
    set x to receive from ch
    """

    ast = _parse(code)
    generator = LLVMIRGenerator()
    llvm_ir = generator.generate(ast)

    assert "declare i8* @nxl_channel_create()" in llvm_ir
    assert "declare void @nxl_channel_send(i8*, i64)" in llvm_ir
    assert "declare i64 @nxl_channel_receive(i8*)" in llvm_ir
    assert "declare void @nxl_channel_close(i8*)" in llvm_ir
    assert "call i8* @nxl_channel_create()" in llvm_ir
    assert "call void @nxl_channel_send(i8*" in llvm_ir
    assert "call i64 @nxl_channel_receive(i8* " in llvm_ir


def test_llvm_channel_float_payload_uses_bitcast_transport():
    code = """
    set ch to create channel
    send 1.5 to ch
    set x to receive from ch
    """

    ast = _parse(code)
    generator = LLVMIRGenerator()
    llvm_ir = generator.generate(ast)

    assert "bitcast double" in llvm_ir
    assert "to i64" in llvm_ir
    assert "bitcast i64" in llvm_ir
    assert "to double" in llvm_ir


def test_llvm_lowers_close_statement_to_runtime_call():
    code = """
    set ch to create channel
    close ch
    """

    ast = _parse(code)
    generator = LLVMIRGenerator()
    llvm_ir = generator.generate(ast)

    assert "declare void @nxl_channel_close(i8*)" in llvm_ir
    assert "call void @nxl_channel_close(i8* " in llvm_ir


def test_llvm_channel_send_retains_rc_payload_and_tracks_received_owner():
    code = """
    set ptr to Rc of Integer with 7
    set ch to create channel
    send ptr to ch
    set received to receive from ch
    """

    ast = _parse(code)
    generator = LLVMIRGenerator()
    llvm_ir = generator.generate(ast)

    assert "call i8* @rc_retain(i8* " in llvm_ir
    assert "call void @nxl_channel_send(i8*" in llvm_ir
    assert "ptrtoint i8*" in llvm_ir
    assert "inttoptr i64" in llvm_ir
    assert generator.channel_payload_ownership["ch"]["kind"] == "rc"
    assert generator.rc_variables["received"]["kind"] == "rc"


def test_llvm_channel_send_retains_arc_payload_and_tracks_received_owner():
    code = """
    set ptr to Arc of Integer with 7
    set ch to create channel
    send ptr to ch
    set received to receive from ch
    """

    ast = _parse(code)
    generator = LLVMIRGenerator()
    llvm_ir = generator.generate(ast)

    assert "call i8* @arc_retain(i8* " in llvm_ir
    assert generator.channel_payload_ownership["ch"]["kind"] == "arc"
    assert generator.rc_variables["received"]["kind"] == "arc"
