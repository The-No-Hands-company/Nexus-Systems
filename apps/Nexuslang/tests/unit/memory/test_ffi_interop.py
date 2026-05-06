"""
Tests for FFI interoperability features:
- ABI compatibility checker (ffi_abi_checker.py)
- FFI debugging tools - call tracer, GDB/LLDB generators (ffi_debug.py)
- C++ interop - name mangling, class wrapping, templates,
                exception handling, RTTI (ffi_cpp.py)
"""

import pytest
import ctypes
import ctypes.util
import json
import os
import platform
import tempfile
from typing import List, Tuple
from unittest.mock import patch

# ---------------------------------------------------------------------------
# Imports under test
# ---------------------------------------------------------------------------
from nexuslang.compiler.ffi_abi_checker import (
    ABICompatibilityChecker,
    ABIPlatform,
    ABICheckResult,
    CheckSeverity,
    CTypesVerifier,
    StructLayoutCalculator,
    FunctionSignatureChecker,
    FunctionSignatureDecl,
    detect_platform,
    _align_up,
    _NLPL_TO_C,
)

from nexuslang.compiler.ffi_debug import (
    FFICallTracer,
    FFICallRecord,
    FFICallStatus,
    AssertionHook,
    GDBScriptGenerator,
    LLDBScriptGenerator,
    FFIDebugger,
    generate_valgrind_command,
)

from nexuslang.compiler.ffi_cpp import (
    CppInterop,
    CppNameMangler,
    ManglingABI,
    ItaniumDemangler,
    CppClassWrapper,
    CppMethod,
    CppWrapperGenerator,
    TemplateInstance,
    TemplateInstantiationHelper,
    CppExceptionBridge,
    CppExceptionClass,
    RTTISupport,
    RTTITypeInfo,
)


# ===========================================================================
# Helper constants
# ===========================================================================

_CURRENT_PLATFORM = detect_platform()

# Simple struct fields used across multiple tests
_POINT_FIELDS: List[Tuple[str, str]] = [("x", "Int32"), ("y", "Int32")]
_MIXED_FIELDS: List[Tuple[str, str]] = [
    ("flag",  "Int8"),
    ("value", "Int32"),
    ("ptr",   "Pointer"),
]


# ===========================================================================
# 1. ABI Compatibility Checker
# ===========================================================================

class TestAlignUp:
    def test_already_aligned(self):
        assert _align_up(8, 4) == 8

    def test_rounds_up(self):
        assert _align_up(5, 4) == 8
        assert _align_up(1, 8) == 8

    def test_zero(self):
        assert _align_up(0, 8) == 0

    def test_alignment_one(self):
        # Alignment 1 means no padding
        assert _align_up(7, 1) == 7


class TestNLPLToCMapping:
    def test_known_types(self):
        assert _NLPL_TO_C["Integer"] == "long long"
        assert _NLPL_TO_C["Float32"] == "float"
        assert _NLPL_TO_C["Boolean"] == "_Bool"
        assert _NLPL_TO_C["Pointer"] == "pointer"

    def test_all_integers_present(self):
        for t in ("Int8", "Int16", "Int32", "Int64",
                  "UInt8", "UInt16", "UInt32", "UInt64"):
            assert t in _NLPL_TO_C, f"{t} missing from _NLPL_TO_C"


class TestStructLayoutCalculator:
    def setup_method(self):
        self.calc = StructLayoutCalculator()

    def test_point_offsets(self):
        layouts, size, alignment = self.calc.compute_layout(_POINT_FIELDS)
        assert layouts[0].offset == 0
        assert layouts[1].offset == 4
        assert size == 8
        assert alignment == 4

    def test_padding_between_char_and_int(self):
        fields = [("c", "Int8"), ("n", "Int32")]
        layouts, size, alignment = self.calc.compute_layout(fields)
        # 'c' at 0, 'n' should be at 4 (3 bytes padding)
        assert layouts[0].offset == 0
        assert layouts[1].offset == 4
        assert size == 8

    def test_packed_no_padding(self):
        fields = [("c", "Int8"), ("n", "Int32")]
        layouts, size, alignment = self.calc.compute_layout(fields, packed=True)
        assert layouts[0].offset == 0
        assert layouts[1].offset == 1
        assert size == 5

    def test_empty_struct(self):
        layouts, size, alignment = self.calc.compute_layout([])
        assert layouts == []
        assert size == 0

    def test_pointer_size(self):
        fields = [("p", "Pointer")]
        layouts, size, alignment = self.calc.compute_layout(fields)
        assert layouts[0].size == 8

    def test_tail_padding_added(self):
        # struct { int8 a; int32 b; } -> size should be 8 (4-aligned), not 5
        fields = [("a", "Int8"), ("b", "Int32")]
        _, size, _ = self.calc.compute_layout(fields)
        assert size == 8

    def test_mixed_field_layout(self):
        layouts, _, _ = self.calc.compute_layout(_MIXED_FIELDS)
        assert layouts[0].offset == 0   # Int8
        assert layouts[1].offset == 4   # Int32 (3 bytes padding)
        assert layouts[2].offset == 8   # Pointer (8-aligned)

    def test_unknown_type_defaults_to_pointer_size(self):
        fields = [("obj", "MyUnknownType")]
        layouts, size, _ = self.calc.compute_layout(fields)
        assert layouts[0].size == 8


class TestCTypesVerifier:
    """Tests that use ctypes to cross-validate struct layouts at runtime."""

    def setup_method(self):
        self.verifier = CTypesVerifier()

    def test_point_struct_no_issues(self):
        issues = self.verifier.verify_struct("Point", _POINT_FIELDS)
        errors = [i for i in issues if i.severity == CheckSeverity.ERROR]
        assert errors == [], f"Unexpected errors: {errors}"

    def test_unknown_type_gives_warning(self):
        fields = [("x", "MyOpaqueType")]
        issues = self.verifier.verify_struct("Opaque", fields)
        warnings = [i for i in issues if i.severity == CheckSeverity.WARNING]
        assert len(warnings) >= 1

    def test_nxl_to_ctypes_int32(self):
        ct = self.verifier.nxl_to_ctypes("Int32")
        assert ct is ctypes.c_int32

    def test_nxl_to_ctypes_pointer(self):
        ct = self.verifier.nxl_to_ctypes("Pointer")
        assert ct is ctypes.c_void_p


class TestFunctionSignatureChecker:
    def test_valid_cdecl_on_sysv(self):
        checker = FunctionSignatureChecker(ABIPlatform.SYSTEM_V_AMD64)
        decl = FunctionSignatureDecl("write", ["Int32", "Pointer", "Int64"], "Int64")
        issues = checker.check_signature(decl)
        errors = [i for i in issues if i.severity == CheckSeverity.ERROR]
        assert errors == []

    def test_stdcall_on_linux_is_error(self):
        checker = FunctionSignatureChecker(ABIPlatform.SYSTEM_V_AMD64)
        decl = FunctionSignatureDecl("foo", [], "Void", calling_convention="stdcall")
        issues = checker.check_signature(decl)
        errors = [i for i in issues if i.severity == CheckSeverity.ERROR]
        assert len(errors) >= 1

    def test_variadic_float_promotion_info(self):
        checker = FunctionSignatureChecker(ABIPlatform.SYSTEM_V_AMD64)
        decl = FunctionSignatureDecl("printf", ["String", "Float32"], "Int32",
                                     is_variadic=True)
        issues = checker.check_signature(decl)
        info = [i for i in issues if i.severity == CheckSeverity.INFO
                and i.category == "float_promotion"]
        assert len(info) >= 1

    def test_integer_promotion_info_for_small_types(self):
        checker = FunctionSignatureChecker(ABIPlatform.SYSTEM_V_AMD64)
        decl = FunctionSignatureDecl("foo", ["Int8"], "Void", is_variadic=True)
        issues = checker.check_signature(decl)
        info = [i for i in issues if i.category == "integer_promotion"]
        assert len(info) >= 1


class TestABICompatibilityChecker:
    def setup_method(self):
        self.checker = ABICompatibilityChecker(ABIPlatform.SYSTEM_V_AMD64)

    def test_simple_point_passes(self):
        result = self.checker.check_struct("Point", _POINT_FIELDS)
        assert result.passed
        assert result.platform == ABIPlatform.SYSTEM_V_AMD64

    def test_empty_struct_warning(self):
        result = self.checker.check_struct("Empty", [])
        warnings = result.warnings()
        assert len(warnings) >= 1
        assert any("Empty" in str(w) for w in warnings)

    def test_struct_with_padding_reports_info(self):
        fields = [("a", "Int8"), ("b", "Int32")]
        result = self.checker.check_struct("PaddedStruct", fields)
        infos = [i for i in result.issues if i.severity == CheckSeverity.INFO
                 and i.category == "struct_padding"]
        assert len(infos) >= 1

    def test_valid_function_signature(self):
        result = self.checker.check_function("open", ["String", "Int32"], "Int32")
        assert result.passed

    def test_invalid_calling_convention_error(self):
        result = self.checker.check_function("foo", [], "Void",
                                              calling_convention="stdcall")
        assert not result.passed
        assert len(result.errors()) >= 1

    def test_enum_valid_range(self):
        values = [("RED", 0), ("GREEN", 1), ("BLUE", 2)]
        result = self.checker.check_enum("Color", values, "Int32")
        assert result.passed

    def test_enum_out_of_range_error(self):
        # Int8 max is 127; 200 exceeds it for unsigned, 200 > 127 for signed
        values = [("A", 0), ("B", 200)]  # 200 overflows signed Int8
        result = self.checker.check_enum("MyEnum", values, "Int8")
        errors = result.errors()
        assert len(errors) >= 1

    def test_enum_non_int_underlying_type_warning(self):
        # If underlying type is not 4 bytes, should warn
        values = [("A", 0)]
        result = self.checker.check_enum("MyEnum", values, "Int64")
        warnings = result.warnings()
        assert len(warnings) >= 1

    def test_get_struct_layout_returns_fields(self):
        layouts = self.checker.get_struct_layout(_POINT_FIELDS)
        assert len(layouts) == 2
        assert layouts[0].name == "x"
        assert layouts[1].name == "y"

    def test_abi_check_result_str(self):
        result = self.checker.check_struct("Point", _POINT_FIELDS)
        s = str(result)
        assert "PASSED" in s or "FAILED" in s

    def test_detect_platform_returns_known_platform(self):
        platform_val = detect_platform()
        valid = list(ABIPlatform)
        assert platform_val in valid


# ===========================================================================
# 2. FFI Debugging Tools
# ===========================================================================

class TestFFICallTracer:

    def setup_method(self):
        self.tracer = FFICallTracer(enabled=True)

    def test_record_single_call(self):
        rec = self.tracer.record(
            function_name="malloc",
            library="libc",
            arguments=[1024],
            return_value=12345,
            status=FFICallStatus.SUCCESS,
            elapsed_ns=500,
        )
        assert rec.function_name == "malloc"
        assert rec.status == FFICallStatus.SUCCESS
        assert rec.elapsed_ns == 500
        assert len(self.tracer.records) == 1

    def test_record_null_return(self):
        self.tracer.record("malloc", None, [0], None, FFICallStatus.NULL_RETURN, 100)
        assert len(self.tracer.null_returns()) == 1

    def test_records_bounded_by_max(self):
        tracer = FFICallTracer(max_records=5)
        for i in range(10):
            tracer.record("f", None, [i], i, FFICallStatus.SUCCESS, 10)
        assert len(tracer.records) == 5

    def test_clear(self):
        self.tracer.record("f", None, [], 0, FFICallStatus.SUCCESS, 0)
        self.tracer.clear()
        assert len(self.tracer.records) == 0

    def test_calls_to(self):
        self.tracer.record("malloc", None, [64], 1, FFICallStatus.SUCCESS, 10)
        self.tracer.record("free", None, [1], None, FFICallStatus.SUCCESS, 5)
        assert len(self.tracer.calls_to("malloc")) == 1
        assert len(self.tracer.calls_to("free")) == 1

    def test_slowest_calls(self):
        for elapsed in [10, 5, 100, 1, 50]:
            self.tracer.record("f", None, [], None, FFICallStatus.SUCCESS, elapsed)
        slowest = self.tracer.slowest_calls(top_n=3)
        assert slowest[0].elapsed_ns == 100
        assert len(slowest) == 3

    def test_disable_skips_recording(self):
        self.tracer.disable()

        def dummy(x):
            return x * 2

        wrapped = self.tracer.wrap_ctypes_function("dummy", dummy)
        result = wrapped(5)
        assert result == 10
        assert len(self.tracer.records) == 0

    def test_wrap_ctypes_function_records(self):
        def add(a, b):
            return a + b

        wrapped = self.tracer.wrap_ctypes_function("add", add)
        result = wrapped(3, 4)
        assert result == 7
        assert len(self.tracer.records) == 1
        rec = self.tracer.records[0]
        assert rec.function_name == "add"
        assert rec.status == FFICallStatus.SUCCESS

    def test_wrap_ctypes_function_captures_exception(self):
        def always_fails():
            raise ValueError("boom")

        wrapped = self.tracer.wrap_ctypes_function("fail_func", always_fails)
        with pytest.raises(ValueError):
            wrapped()
        assert len(self.tracer.records) == 1
        assert self.tracer.records[0].status == FFICallStatus.EXCEPTION

    def test_post_assertion_failure_marks_record(self):
        self.tracer.add_post_assertion(
            "no_zero", "divide",
            lambda fn, rv: "result is zero" if rv == [0] else None
        )

        def divide(a, b):
            return a // b

        wrapped = self.tracer.wrap_ctypes_function("divide", divide)
        wrapped(0, 10)
        rec = self.tracer.records[0]
        assert not rec.post_assertions_passed

    def test_pre_assertion_failure_marks_record(self):
        self.tracer.add_pre_assertion(
            "no_negative", "sqrt",
            lambda fn, args: "negative input" if args and args[0] < 0 else None
        )

        def sqrt_dummy(x):
            return x

        wrapped = self.tracer.wrap_ctypes_function("sqrt", sqrt_dummy)
        wrapped(-1)
        rec = self.tracer.records[0]
        assert not rec.pre_assertions_passed

    def test_failed_calls_filters(self):
        def ok():
            return 1

        def fail():
            raise RuntimeError("err")

        wrapped_ok = self.tracer.wrap_ctypes_function("ok", ok)
        wrapped_fail = self.tracer.wrap_ctypes_function("fail", fail)

        wrapped_ok()
        with pytest.raises(RuntimeError):
            wrapped_fail()

        failed = self.tracer.failed_calls()
        assert len(failed) == 1
        assert failed[0].function_name == "fail"

    def test_export_and_import_json(self):
        self.tracer.record("testfn", "lib", [1, 2], 3, FFICallStatus.SUCCESS, 42)
        with tempfile.NamedTemporaryFile(mode='w', suffix='.json', delete=False) as f:
            path = f.name

        try:
            self.tracer.export_json(path)
            assert os.path.exists(path)

            new_tracer = FFICallTracer()
            new_tracer.import_json(path)
            assert len(new_tracer.records) == 1
            assert new_tracer.records[0].function_name == "testfn"
        finally:
            os.unlink(path)

    def test_call_record_to_dict(self):
        rec = self.tracer.record("fn", "lib", [b"bytes", 42], "ok",
                                 FFICallStatus.SUCCESS, 10)
        d = rec.to_dict()
        assert d["function_name"] == "fn"
        assert d["status"] == "success"
        # bytes should be serialized as hex dict
        assert isinstance(d["arguments"][0], dict)
        assert "__bytes__" in d["arguments"][0]

    def test_listener_called(self):
        received = []
        self.tracer.add_listener(lambda r: received.append(r.function_name))
        self.tracer.record("f", None, [], None, FFICallStatus.SUCCESS, 0)
        assert received == ["f"]


class TestGDBScriptGenerator:

    def setup_method(self):
        self.gen = GDBScriptGenerator()

    def test_generate_contains_program_path(self):
        self.gen.add_function_breakpoint("malloc")
        script = self.gen.generate("/path/to/program")
        assert "malloc" in script
        assert "/path/to/program" in script

    def test_generate_contains_breakpoint(self):
        self.gen.add_function_breakpoint("free", log_args=True)
        script = self.gen.generate("./prog")
        assert "break free" in script

    def test_add_watchpoint(self):
        self.gen.add_watchpoint("my_global_var")
        script = self.gen.generate("./prog")
        assert "watch my_global_var" in script

    def test_write_to_file(self):
        with tempfile.NamedTemporaryFile(suffix='.gdb', delete=False) as f:
            path = f.name
        try:
            self.gen.add_function_breakpoint("malloc")
            self.gen.write(path, "./prog")
            with open(path) as fh:
                content = fh.read()
            assert "malloc" in content
            assert "gdb" in content.lower() or "GDB" in content or "break" in content
        finally:
            os.unlink(path)

    def test_conditional_breakpoint(self):
        self.gen.add_function_breakpoint("malloc", condition="size > 1024")
        script = self.gen.generate("./prog")
        assert "size > 1024" in script


class TestLLDBScriptGenerator:

    def setup_method(self):
        self.gen = LLDBScriptGenerator()

    def test_generate_contains_program(self):
        self.gen.add_function_breakpoint("free")
        script = self.gen.generate("/bin/myapp")
        assert "/bin/myapp" in script

    def test_generate_contains_breakpoint(self):
        self.gen.add_function_breakpoint("malloc")
        script = self.gen.generate("./prog")
        assert "malloc" in script

    def test_write_to_file(self):
        with tempfile.NamedTemporaryFile(suffix='.lldb', delete=False) as f:
            path = f.name
        try:
            self.gen.add_function_breakpoint("malloc")
            self.gen.write(path, "./prog")
            with open(path) as fh:
                content = fh.read()
            assert "malloc" in content
        finally:
            os.unlink(path)


class TestValgrindCommand:
    def test_basic_command(self):
        cmd = generate_valgrind_command("/path/prog")
        assert "valgrind" in cmd
        assert "/path/prog" in cmd
        assert "--tool=memcheck" in cmd

    def test_full_leak_check(self):
        cmd = generate_valgrind_command("./prog", leak_check="full")
        assert "--leak-check=full" in cmd

    def test_xml_output(self):
        cmd = generate_valgrind_command("./prog", output_xml="/tmp/out.xml")
        assert "--xml=yes" in cmd
        assert "/tmp/out.xml" in cmd

    def test_track_origins(self):
        cmd = generate_valgrind_command("./prog", track_origins=True)
        assert "--track-origins=yes" in cmd

    def test_program_args(self):
        cmd = generate_valgrind_command("./prog", program_args=["--port", "8080"])
        assert "--port" in cmd
        assert "8080" in cmd


class TestFFIDebugger:
    def test_trace_wraps_function(self):
        dbg = FFIDebugger(program_path="./prog")
        result_store = []

        def my_func(x):
            result_store.append(x)
            return x * 2

        traced = dbg.trace(my_func, "my_func")
        assert traced(21) == 42
        assert result_store == [21]
        assert len(dbg.tracer.records) == 1

    def test_gdb_script_written(self):
        dbg = FFIDebugger(program_path="./prog")
        dbg.watch_function("malloc")
        with tempfile.NamedTemporaryFile(suffix='.gdb', delete=False) as f:
            path = f.name
        try:
            dbg.write_gdb_script(path)
            with open(path) as fh:
                content = fh.read()
            assert "malloc" in content
        finally:
            os.unlink(path)


# ===========================================================================
# 3. C++ Interop
# ===========================================================================

class TestItaniumDemangler:

    def _demangle(self, sym: str) -> str:
        return ItaniumDemangler(sym).demangle()

    def test_not_mangled_passthrough(self):
        assert self._demangle("printf") == "printf"

    def test_simple_free_function(self):
        # _Z3foov -> foo()
        result = self._demangle("_Z3foov")
        assert "foo" in result

    def test_namespaced_function(self):
        # _ZN3foo3barEv -> foo::bar
        result = self._demangle("_ZN3foo3barEv")
        assert "foo" in result
        assert "bar" in result

    def test_nested_namespace(self):
        # _ZN3foo3bar3bazEv
        result = self._demangle("_ZN3foo3bar3bazEv")
        assert "foo" in result
        assert "bar" in result
        assert "baz" in result

    def test_constructor(self):
        # _ZN5ClassC1Ev -> Class::(constructor)
        result = self._demangle("_ZN5ClassC1Ev")
        assert "Class" in result

    def test_destructor(self):
        # _ZN5ClassD1Ev
        result = self._demangle("_ZN5ClassD1Ev")
        assert "Class" in result

    def test_template_instance(self):
        # _ZN3std6vectorIiEC1Ev (simplified)
        result = self._demangle("_ZN3std6vectorIiEC1Ev")
        assert "std" in result
        assert "vector" in result

    def test_standard_substitution_St(self):
        # Any symbol with 'St' prefix should decode the namespace
        result = self._demangle("_ZStlsRSoRKSs")
        # Should include 'std' or similar
        assert result  # At minimum, should not crash

    def test_invalid_symbol_returned_as_is(self):
        assert self._demangle("completely_invalid_xyz") == "completely_invalid_xyz"


class TestCppNameMangler:

    def setup_method(self):
        self.mangler = CppNameMangler(ManglingABI.ITANIUM)

    def test_demangle_via_python_fallback(self):
        result = self.mangler.demangle("_Z3foov")
        assert "foo" in result

    def test_mangle_function_no_namespace_no_class(self):
        mangled = self.mangler.mangle_function("", None, "hello")
        assert "hello" in mangled

    def test_mangle_function_with_namespace(self):
        mangled = self.mangler.mangle_function("myns", None, "func")
        assert "myns" in mangled
        assert "func" in mangled

    def test_mangle_function_with_class(self):
        mangled = self.mangler.mangle_function("", "MyClass", "method")
        assert "MyClass" in mangled
        assert "method" in mangled

    def test_demangle_batch(self):
        symbols = ["_Z3foov", "_ZN3foo3barEv", "not_mangled"]
        result = self.mangler.demangle_batch(symbols)
        assert len(result) == 3
        assert "not_mangled" in result
        assert result["not_mangled"] == "not_mangled"


class TestCppWrapperGenerator:

    def setup_method(self):
        self.gen = CppWrapperGenerator(exception_handling=True)
        self.wrapper = CppClassWrapper(
            class_name="Widget",
            namespace="ui",
            methods=[
                CppMethod("width",  [],        "int"),
                CppMethod("resize", ["int", "int"], "void"),
                CppMethod("name",   [],        "std::string", is_const=True),
            ],
            constructor_params=[],
            include_header="widget.h",
        )

    def test_header_contains_typedef(self):
        header = self.gen.generate_header(self.wrapper)
        assert "nxl_widget_t" in header

    def test_header_contains_new_func(self):
        header = self.gen.generate_header(self.wrapper)
        assert "nxl_widget_new" in header

    def test_header_contains_delete_func(self):
        header = self.gen.generate_header(self.wrapper)
        assert "nxl_widget_delete" in header

    def test_header_contains_method_declarations(self):
        header = self.gen.generate_header(self.wrapper)
        assert "nxl_widget_width" in header
        assert "nxl_widget_resize" in header
        assert "nxl_widget_name" in header

    def test_header_has_cpp_guard(self):
        header = self.gen.generate_header(self.wrapper)
        assert '#ifdef __cplusplus' in header
        assert 'extern "C"' in header

    def test_header_has_include_guard(self):
        header = self.gen.generate_header(self.wrapper)
        assert '#ifndef NLPL_WIDGET_WRAPPER_H' in header

    def test_impl_contains_constructor(self):
        impl = self.gen.generate_implementation(self.wrapper)
        assert "nxl_widget_new" in impl

    def test_impl_contains_destructor(self):
        impl = self.gen.generate_implementation(self.wrapper)
        assert "nxl_widget_delete" in impl

    def test_impl_contains_exception_catch(self):
        impl = self.gen.generate_implementation(self.wrapper)
        assert "std::exception" in impl

    def test_impl_contains_extern_c(self):
        impl = self.gen.generate_implementation(self.wrapper)
        assert 'extern "C"' in impl

    def test_method_names_lowercased(self):
        assert self.wrapper.c_method_name("Width") == "nxl_widget_width"

    def test_qualified_name(self):
        assert self.wrapper.qualified_name == "ui::Widget"

    def test_c_type_name(self):
        assert self.wrapper.c_type_name == "nxl_widget_t"

    def test_wrapper_without_exception_handling(self):
        gen = CppWrapperGenerator(exception_handling=False)
        impl = gen.generate_implementation(self.wrapper)
        assert "std::exception" not in impl

    def test_cpp_type_to_c_string(self):
        assert self.gen._cpp_type_to_c("std::string") == "const char*"

    def test_cpp_type_to_c_bool(self):
        assert self.gen._cpp_type_to_c("bool") == "int"


class TestTemplateInstantiationHelper:

    def setup_method(self):
        self.helper = TemplateInstantiationHelper()

    def test_register_returns_alias(self):
        inst = TemplateInstance("vector", ["int"], namespace="std", kind="class")
        alias = self.helper.register(inst)
        assert "vector" in alias.lower()
        assert alias.startswith("nxl_tmpl_")

    def test_register_custom_alias(self):
        inst = TemplateInstance("vector", ["int"], namespace="std",
                                wrapper_func_name="my_int_vec")
        alias = self.helper.register(inst)
        assert alias == "my_int_vec"

    def test_generate_header_contains_alias(self):
        inst = TemplateInstance("map", ["string", "int"], namespace="std", kind="class")
        alias = self.helper.register(inst)
        header = self.helper.generate_instantiation_header(inst)
        assert alias in header

    def test_generate_impl_contains_explicit_instantiation(self):
        inst = TemplateInstance("vector", ["int"], namespace="std", kind="class")
        impl = self.helper.generate_instantiation_impl(inst)
        assert "template class" in impl
        assert "std::vector<int>" in impl

    def test_generate_impl_contains_extern_c(self):
        inst = TemplateInstance("vector", ["int"], namespace="std", kind="class")
        impl = self.helper.generate_instantiation_impl(inst)
        assert 'extern "C"' in impl

    def test_list_instances(self):
        inst = TemplateInstance("set", ["float"], namespace="std", kind="class")
        self.helper.register(inst)
        instances = self.helper.list_instances()
        assert len(instances) >= 1


class TestCppExceptionBridge:

    def setup_method(self):
        self.bridge = CppExceptionBridge()

    def test_generate_infrastructure_contains_thread_local(self):
        src = self.bridge.generate_thread_local_exception_infrastructure()
        assert "thread_local" in src

    def test_generate_infrastructure_contains_clear_func(self):
        src = self.bridge.generate_thread_local_exception_infrastructure()
        assert "nxl_ffi_clear_exception" in src

    def test_generate_infrastructure_contains_get_func(self):
        src = self.bridge.generate_thread_local_exception_infrastructure()
        assert "nxl_ffi_get_exception" in src

    def test_generate_infrastructure_contains_macros(self):
        src = self.bridge.generate_thread_local_exception_infrastructure()
        assert "NLPL_TRY_CALL" in src
        assert "NLPL_TRY_CALL_VOID" in src

    def test_generate_infrastructure_catches_bad_alloc(self):
        src = self.bridge.generate_thread_local_exception_infrastructure()
        assert "std::bad_alloc" in src

    def test_exception_code_for_known_class(self):
        assert self.bridge.exception_code_for(CppExceptionClass.STD_BAD_ALLOC) == 4
        assert self.bridge.exception_code_for(CppExceptionClass.STD_EXCEPTION) == 1

    def test_exception_code_for_unknown(self):
        assert self.bridge.exception_code_for(CppExceptionClass.UNKNOWN) == 99

    def test_exception_class_for_code_roundtrip(self):
        for exc in CppExceptionClass:
            code = self.bridge.exception_code_for(exc)
            recovered = self.bridge.exception_class_for_code(code)
            assert recovered == exc


class TestRTTISupport:

    def setup_method(self):
        self.rtti = RTTISupport()
        self.rtti.register_type(RTTITypeInfo("Animal", "_ZTI6Animal", is_polymorphic=True))
        self.rtti.register_type(RTTITypeInfo("Dog", "_ZTI3Dog", base_classes=["Animal"]))
        self.rtti.register_type(RTTITypeInfo("Cat", "_ZTI3Cat", base_classes=["Animal"]))
        self.rtti.register_type(RTTITypeInfo("Labrador", "_ZTI8Labrador", base_classes=["Dog"]))

    def test_is_subclass_direct(self):
        assert self.rtti.is_subclass("Dog", "Animal")
        assert self.rtti.is_subclass("Cat", "Animal")

    def test_is_subclass_transitive(self):
        assert self.rtti.is_subclass("Labrador", "Animal")

    def test_is_not_subclass(self):
        assert not self.rtti.is_subclass("Dog", "Cat")
        assert not self.rtti.is_subclass("Animal", "Dog")

    def test_is_subclass_unknown_type(self):
        assert not self.rtti.is_subclass("Unknown", "Animal")

    def test_generate_rtti_wrappers_contains_dynamic_cast(self):
        src = self.rtti.generate_rtti_wrappers("Animal", ["Dog", "Cat"])
        assert "dynamic_cast" in src

    def test_generate_rtti_wrappers_contains_typeid(self):
        src = self.rtti.generate_rtti_wrappers("Animal", ["Dog", "Cat"])
        assert "typeid" in src

    def test_generate_rtti_wrappers_contains_both_derived(self):
        src = self.rtti.generate_rtti_wrappers("Animal", ["Dog", "Cat"])
        assert "Dog" in src
        assert "Cat" in src

    def test_generate_rtti_wrappers_isa_functions(self):
        src = self.rtti.generate_rtti_wrappers("Animal", ["Dog", "Cat"])
        assert "nxl_rtti_isa_dog" in src
        assert "nxl_rtti_isa_cat" in src

    def test_generate_rtti_header_has_include_guard(self):
        header = self.rtti.generate_rtti_header("Animal", ["Dog"])
        assert "#ifndef NLPL_RTTI_ANIMAL_H" in header

    def test_generate_rtti_header_has_extern_c(self):
        header = self.rtti.generate_rtti_header("Animal", ["Dog"])
        assert 'extern "C"' in header

    def test_get_type_info(self):
        info = self.rtti.get_type_info("Dog")
        assert info is not None
        assert info.mangled_name == "_ZTI3Dog"


class TestCppInteropFacade:
    """Tests for the high-level CppInterop convenience API."""

    def setup_method(self):
        self.cpp = CppInterop(ManglingABI.ITANIUM)

    def test_demangle_passthrough(self):
        assert self.cpp.demangle("printf") == "printf"

    def test_demangle_mangled(self):
        result = self.cpp.demangle("_ZN3foo3barEv")
        assert "foo" in result and "bar" in result

    def test_demangle_batch(self):
        result = self.cpp.demangle_batch(["_Z3foov", "main"])
        assert "foo" in result["_Z3foov"]
        assert result["main"] == "main"

    def test_generate_class_header(self):
        wrapper = CppClassWrapper("Greeter", methods=[
            CppMethod("greet", ["String"], "void"),
        ])
        header = self.cpp.generate_class_header(wrapper)
        assert "nxl_greeter_t" in header

    def test_generate_class_impl(self):
        wrapper = CppClassWrapper("Greeter")
        impl = self.cpp.generate_class_impl(wrapper)
        assert "nxl_greeter_new" in impl

    def test_register_and_generate_template(self):
        inst = TemplateInstance("vector", ["double"], namespace="std", kind="class")
        alias = self.cpp.register_template(inst)
        header = self.cpp.generate_template_header(inst)
        impl = self.cpp.generate_template_impl(inst)
        assert alias in header
        assert "std::vector<double>" in impl

    def test_generate_exception_infrastructure(self):
        src = self.cpp.generate_exception_infrastructure()
        assert "nxl_ffi_clear_exception" in src

    def test_exception_code(self):
        code = self.cpp.exception_code(CppExceptionClass.STD_BAD_ALLOC)
        assert code == 4

    def test_exception_class_roundtrip(self):
        code = self.cpp.exception_code(CppExceptionClass.STD_RUNTIME_ERROR)
        recovered = self.cpp.exception_class_for_code(code)
        assert recovered == CppExceptionClass.STD_RUNTIME_ERROR

    def test_register_rtti_and_generate(self):
        self.cpp.register_rtti_type(
            RTTITypeInfo("Shape", "_ZTI5Shape", base_classes=[])
        )
        self.cpp.register_rtti_type(
            RTTITypeInfo("Circle", "_ZTI6Circle", base_classes=["Shape"])
        )
        wrappers = self.cpp.generate_rtti_wrappers("Shape", ["Circle"])
        assert "dynamic_cast" in wrappers

    def test_is_subclass(self):
        self.cpp.register_rtti_type(RTTITypeInfo("Base", "_ZTI4Base"))
        self.cpp.register_rtti_type(RTTITypeInfo("Sub",  "_ZTI3Sub", base_classes=["Base"]))
        assert self.cpp.is_subclass("Sub", "Base")
        assert not self.cpp.is_subclass("Base", "Sub")

    def test_generate_rtti_header(self):
        header = self.cpp.generate_rtti_header("Shape", ["Circle", "Square"])
        assert "NLPL_RTTI_SHAPE_H" in header
        # Generated function names are lowercased (e.g. cast_shape_to_circle)
        assert "circle" in header
        assert "square" in header
