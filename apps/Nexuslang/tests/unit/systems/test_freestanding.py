"""Tests for the freestanding / bare-metal compilation mode."""
import pytest
import sys
import os
import tempfile

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'src'))

from nexuslang.compiler.freestanding import (
    FreestandingConfig,
    FreestandingViolation,
    FREESTANDING_FORBIDDEN_MODULES,
    FREESTANDING_ALLOWED_MODULES,
    ARCH_STUBS,
    LINKER_SCRIPT_TEMPLATES,
    generate_entry_stub,
    generate_linker_script,
    parse_freestanding_args,
    apply_freestanding_config,
)
from nexuslang.compiler.linker import (
    LinkerScriptConfig,
    LinkerScriptValidator,
    MemoryRegion,
    LinkerSection,
    cortex_m_config,
    x86_flat_config,
    riscv_config,
    get_linker_script_for_arch,
    find_linker,
    LINKER_PRESETS,
)


# ---------------------------------------------------------------------------
# Entry stub generation
# ---------------------------------------------------------------------------

class TestEntryStubb:
    def test_x86_stub(self):
        stub = generate_entry_stub('x86')
        assert '_start' in stub
        assert 'esp' in stub.lower() or 'mov' in stub.lower()

    def test_x86_64_stub(self):
        stub = generate_entry_stub('x86_64')
        assert '_start' in stub
        assert 'nxl_main' in stub

    def test_arm_cortex_m_stub(self):
        stub = generate_entry_stub('cortex-m')
        assert 'Reset_Handler' in stub
        assert '__stack_top' in stub

    def test_riscv_stub(self):
        stub = generate_entry_stub('riscv')
        assert '_start' in stub
        assert 'la' in stub or 'lw' in stub or 'sw' in stub

    def test_all_stubs_reference_nxl_main(self):
        for arch in ARCH_STUBS:
            stub = generate_entry_stub(arch)
            assert 'nxl_main' in stub or 'nlpl' in stub.lower(), \
                f"Entry stub for '{arch}' does not reference nxl_main"

    def test_unsupported_arch_raises(self):
        with pytest.raises(ValueError, match="unsupported architecture"):
            generate_entry_stub('mips')


# ---------------------------------------------------------------------------
# Linker script generation
# ---------------------------------------------------------------------------

class TestLinkerScriptGeneration:
    def test_x86_64_script(self):
        script = generate_linker_script('x86_64')
        assert 'MEMORY' in script
        assert 'SECTIONS' in script
        assert 'ENTRY' in script
        assert '__stack_top' in script

    def test_cortex_m_script(self):
        script = generate_linker_script('cortex-m')
        assert 'FLASH' in script
        assert 'SRAM' in script
        assert 'Reset_Handler' in script

    def test_riscv_script(self):
        script = generate_linker_script('riscv32')
        assert 'FLASH' in script
        assert '_start' in script

    def test_all_templates_have_memory_sections(self):
        for arch in LINKER_SCRIPT_TEMPLATES:
            script = generate_linker_script(arch)
            assert 'MEMORY' in script, f"Script for '{arch}' missing MEMORY block"
            assert 'SECTIONS' in script, f"Script for '{arch}' missing SECTIONS block"
            assert '__bss_start' in script, f"Script for '{arch}' missing __bss_start"
            assert '__bss_end' in script, f"Script for '{arch}' missing __bss_end"

    def test_unsupported_arch_raises(self):
        with pytest.raises(ValueError, match="no linker script template"):
            generate_linker_script('mips')


# ---------------------------------------------------------------------------
# FreestandingConfig
# ---------------------------------------------------------------------------

class TestFreestandingConfig:
    def _make_config(self, **kwargs) -> FreestandingConfig:
        defaults = dict(
            arch='x86_64',
            entry_symbol='nxl_main',
            stack_size=65536,
            heap_size=131072,
        )
        defaults.update(kwargs)
        return FreestandingConfig(**defaults)

    def test_default_forbidden_modules(self):
        cfg = self._make_config()
        forbidden = cfg.forbidden_modules()
        assert 'network' in forbidden
        assert 'kernel' in forbidden
        assert 'subprocess_utils' in forbidden

    def test_threading_forbidden_by_default(self):
        cfg = self._make_config(enable_threads=False)
        forbidden = cfg.forbidden_modules()
        assert 'threading' in forbidden

    def test_threading_allowed_when_enabled(self):
        cfg = self._make_config(enable_threads=True)
        forbidden = cfg.forbidden_modules()
        assert 'threading' not in forbidden

    def test_validate_allowed_module_passes(self):
        cfg = self._make_config()
        # math is always allowed
        cfg.validate_module_use('math')  # should not raise

    def test_validate_forbidden_module_raises(self):
        cfg = self._make_config()
        with pytest.raises(FreestandingViolation, match="not available"):
            cfg.validate_module_use('network')

    def test_emit_artefacts_entry_stub(self):
        with tempfile.NamedTemporaryFile(suffix='.s', delete=False) as f:
            stub_path = f.name
        try:
            cfg = self._make_config(arch='x86_64', output_entry_stub=stub_path)
            written = cfg.emit_artefacts()
            assert stub_path in written
            with open(stub_path) as f:
                content = f.read()
            assert 'nxl_main' in content
        finally:
            os.unlink(stub_path)

    def test_emit_artefacts_linker_script(self):
        with tempfile.NamedTemporaryFile(suffix='.ld', delete=False) as f:
            ld_path = f.name
        try:
            cfg = self._make_config(arch='x86_64', output_linker_script=ld_path)
            written = cfg.emit_artefacts()
            assert ld_path in written
            with open(ld_path) as f:
                content = f.read()
            assert 'MEMORY' in content
        finally:
            os.unlink(ld_path)

    def test_allowed_modules_superset_of_bare_minimum(self):
        # These modules are essential for any useful bare-metal program
        essential = {'math', 'string', 'collections', 'algorithms', 'bit_ops'}
        assert essential <= FREESTANDING_ALLOWED_MODULES, \
            f"Missing essential allowed modules: {essential - FREESTANDING_ALLOWED_MODULES}"


# ---------------------------------------------------------------------------
# Linker script config & generation
# ---------------------------------------------------------------------------

class TestLinkerScriptConfig:
    def test_custom_config_generation(self):
        cfg = LinkerScriptConfig(entry_symbol='_start')
        cfg.add_region('FLASH', 0x08000000, 256 * 1024, 'rx')
        cfg.add_region('SRAM', 0x20000000, 64 * 1024, 'rwx')
        cfg.add_section('.text', 'FLASH')
        cfg.add_section('.bss', 'SRAM')
        script = cfg.generate()
        assert 'ENTRY(_start)' in script
        assert 'FLASH' in script
        assert 'SRAM' in script
        assert '__bss_start' in script
        assert '__bss_end' in script
        assert '__stack_top' in script

    def test_memory_region_formats(self):
        r = MemoryRegion('ROM', 0x00100000, 512 * 1024, 'rx')
        text = r.to_linker_script()
        assert 'ROM' in text
        assert '0x00100000' in text
        assert '512K' in text

    def test_cortex_m_preset(self):
        cfg = cortex_m_config()
        script = cfg.generate()
        assert 'Reset_Handler' in script
        assert '0x08000000' in script
        assert '0x20000000' in script

    def test_x86_preset(self):
        cfg = x86_flat_config()
        script = cfg.generate()
        assert '_start' in script

    def test_riscv_preset(self):
        cfg = riscv_config()
        script = cfg.generate()
        assert '_start' in script

    def test_all_presets_callable(self):
        for arch_name, factory in LINKER_PRESETS.items():
            cfg = factory()
            script = cfg.generate()
            assert 'SECTIONS' in script, f"Preset '{arch_name}' missing SECTIONS"


# ---------------------------------------------------------------------------
# Linker script validator
# ---------------------------------------------------------------------------

class TestLinkerScriptValidator:
    def test_valid_script(self):
        script = get_linker_script_for_arch('x86_64')
        valid, warnings = LinkerScriptValidator.validate(script)
        assert valid is True
        # The generated script is complete; should produce no critical warnings
        # (only maybe warnings about ENTRY or similar, which are non-fatal)

    def test_minimal_invalid_script(self):
        script = "/* empty */"
        valid, warnings = LinkerScriptValidator.validate(script)
        assert valid is False
        assert len(warnings) > 0

    def test_script_without_bss_symbols(self):
        script = textwrap.dedent("""\
            ENTRY(_start)
            MEMORY { ROM (rx) : ORIGIN = 0x0, LENGTH = 64K }
            SECTIONS { .text : { *(.text) } > ROM }
        """)
        _, warnings = LinkerScriptValidator.validate(script)
        warning_text = ' '.join(warnings)
        assert 'bss_start' in warning_text or 'bss' in warning_text.lower()

    def test_validate_file(self):
        with tempfile.NamedTemporaryFile(mode='w', suffix='.ld', delete=False) as f:
            f.write(get_linker_script_for_arch('cortex-m'))
            path = f.name
        try:
            valid, warnings = LinkerScriptValidator.validate_file(path)
            assert valid is True
        finally:
            os.unlink(path)


# Need textwrap for one of the tests
import textwrap


# ---------------------------------------------------------------------------
# parse_freestanding_args
# ---------------------------------------------------------------------------

class TestParseFreestandingArgs:
    def _make_args(self, **kwargs):
        """Create a minimal args namespace for parse_freestanding_args."""
        import argparse
        ns = argparse.Namespace()
        # Defaults
        ns.freestanding = False
        ns.arch = 'x86_64'
        ns.linker_script = None
        ns.entry_symbol = 'nxl_main'
        ns.stack_size = 65536
        ns.heap_size = 131072
        ns.emit_entry_stub = None
        ns.emit_linker_script = None
        ns.no_float = False
        ns.no_exceptions = False
        ns.bare_metal_threads = False
        ns.debug = False
        for k, v in kwargs.items():
            setattr(ns, k, v)
        return ns

    def test_not_freestanding_returns_none(self):
        args = self._make_args(freestanding=False)
        result = parse_freestanding_args(args)
        assert result is None

    def test_freestanding_returns_config(self):
        args = self._make_args(freestanding=True)
        config = parse_freestanding_args(args)
        assert config is not None
        assert isinstance(config, FreestandingConfig)

    def test_arch_is_propagated(self):
        args = self._make_args(freestanding=True, arch='cortex-m')
        config = parse_freestanding_args(args)
        assert config.arch == 'cortex-m'

    def test_stack_size_is_propagated(self):
        args = self._make_args(freestanding=True, stack_size=8192)
        config = parse_freestanding_args(args)
        assert config.stack_size == 8192

    def test_threads_disabled_by_default(self):
        args = self._make_args(freestanding=True, bare_metal_threads=False)
        config = parse_freestanding_args(args)
        assert config.enable_threads is False

    def test_threads_enabled(self):
        args = self._make_args(freestanding=True, bare_metal_threads=True)
        config = parse_freestanding_args(args)
        assert config.enable_threads is True
