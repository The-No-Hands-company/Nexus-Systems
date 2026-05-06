import pytest

llvmlite = pytest.importorskip("llvmlite")
ir = llvmlite.ir

from nexuslang.compiler.ffi_advanced import CallbackManager
from nexuslang.compiler.ffi_advanced import StringConverter


def test_compiler_callback_registration_generates_callthrough_trampoline():
    module = ir.Module(name="test_module")
    function = ir.Function(module, ir.FunctionType(ir.VoidType(), []), name="entry")
    block = function.append_basic_block(name="entry")
    builder = ir.IRBuilder(block)

    callback_type = ir.FunctionType(ir.IntType(64), [ir.IntType(64), ir.IntType(64)])
    ir.Function(module, callback_type, name="compare_values")

    manager = CallbackManager(module, builder)
    trampoline = manager.register_callback(
        name="sort_callback",
        param_types=["Integer", "Integer"],
        return_type="Integer",
        nxl_function="compare_values",
    )

    assert trampoline is manager.get_callback_pointer("sort_callback")
    ir_text = str(module)
    assert "define i64 @callback_trampoline_sort_callback" in ir_text
    assert "call i64 @compare_values" in ir_text


def test_string_converter_generates_utf8_strlen_logic():
    module = ir.Module(name="strlen_module")
    fn = ir.Function(module, ir.FunctionType(ir.VoidType(), []), name="entry")
    block = fn.append_basic_block(name="entry")
    builder = ir.IRBuilder(block)

    StringConverter(module, builder)

    ir_text = str(module)
    assert "define internal i64 @nxl_strlen" in ir_text
    # UTF-8 continuation detection mask: 0xC0 / 0x80
    assert "and i8" in ir_text
    assert "192" in ir_text
    assert "128" in ir_text