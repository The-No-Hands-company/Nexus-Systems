"""
Standard library for the NexusLang (NexusLang).
This package contains built-in functions and modules for NexusLang programs.
"""

import logging

from ..runtime.runtime import Runtime
from ..stdlib.option_result import register_option_result_functions
from ..stdlib.math import register_math_functions
from ..stdlib.string import register_string_functions
from ..stdlib.stringbuilder import register_stringbuilder_functions
from ..stdlib.io import register_io_functions
from ..stdlib.system import register_system_functions
from ..stdlib.collections import register_collections_functions
from ..stdlib.network import register_network_functions
from ..stdlib.types import register_type_functions
from ..stdlib.asyncio_utils import register_async_functions
from ..stdlib.iterators import register_iterator_functions
from ..stdlib.ffi import register_ffi_functions
from ..stdlib.asm import register_asm_functions
from ..stdlib.testing import register_testing_functions
from ..stdlib.benchmark import register_benchmark_functions
from ..stdlib.graphics import register_graphics_functions
from ..stdlib.modules import register_module_functions
from ..stdlib.filesystem import register_filesystem_functions
from ..stdlib.json_utils import register_json_functions
from ..stdlib.regex import register_regex_functions
from ..stdlib.datetime_utils import register_datetime_functions
from ..stdlib.http import register_http_functions
from ..stdlib.crypto import register_crypto_functions
from ..stdlib.uuid_utils import register_uuid_functions
from ..stdlib.random_utils import register_random_functions
from ..stdlib.compression import register_compression_functions
from ..stdlib.env import register_env_functions
from ..stdlib.subprocess_utils import register_subprocess_functions
from ..stdlib.math3d import register_math3d_functions
from ..stdlib.camera import register_camera_functions
from ..stdlib.shaders import register_shader_functions
from ..stdlib.mesh_loader import register_mesh_functions
from ..stdlib.scene import register_scene_functions
from ..stdlib.logging_utils import register_logging_functions
from ..stdlib.csv_utils import register_csv_functions
from ..stdlib.path_utils import register_path_functions
from ..stdlib.signal_utils import register_signal_functions
from ..stdlib.argparse_utils import register_argparse_functions
from ..stdlib.config import register_config_functions
from ..stdlib.file_io import register_file_io_functions
from ..stdlib.sqlite import register_sqlite_functions
from ..stdlib.xml_utils import register_xml_functions
from ..stdlib.email_utils import register_email_functions
from ..stdlib.templates import register_template_functions
from ..stdlib.threading_utils import register_threading_functions
from ..stdlib.serialization import register_serialization_functions
from ..stdlib.websocket_utils import register_websocket_functions
from ..stdlib.databases import register_database_functions
from ..stdlib.pdf_utils import register_pdf_functions
from ..stdlib.image_utils import register_image_functions
from ..stdlib.validation import register_validation_functions
from ..stdlib.cache import register_cache_functions
from ..stdlib.statistics import register_statistics_functions
from ..stdlib.bit_ops import register_bit_ops_functions
from ..stdlib.ctype import register_ctype_functions
from ..stdlib.limits import register_limits_functions
from ..stdlib.algorithms import register_algorithms_functions
from ..stdlib.errno import register_errno_functions
from ..stdlib.simd import register_simd_functions
from ..stdlib.interrupts import register_interrupt_functions
from ..stdlib.type_traits import register_type_trait_functions
from ..stdlib.hardware import register_stdlib as register_hardware_functions
from ..stdlib.atomics import register_stdlib as register_atomics_functions
from ..stdlib.allocators import register_stdlib as register_allocator_functions
from ..stdlib.smart_pointers import register_stdlib as register_smart_pointer_functions
from ..stdlib.threading import register_stdlib as register_native_threading_functions
from ..stdlib.sync import register_stdlib as register_sync_functions
from ..stdlib.business import register_business_functions
from ..stdlib.data import register_data_functions
from ..stdlib.scientific import register_scientific_functions
from ..stdlib.linalg import register_linalg_functions
from ..stdlib.numint import register_numint_functions
from ..stdlib.dsp import register_dsp_functions
from ..stdlib.plot import register_plot_functions
from ..stdlib.audio import register_audio_functions
from ..stdlib.result_utils import register_result_utils_functions
from ..stdlib.property_testing import register_property_testing_functions
from ..stdlib.coverage_utils import register_coverage_utils_functions
from ..stdlib.parallel import register_parallel_functions
from ..stdlib.kernel import register_kernel_functions
from ..stdlib.drivers import register_driver_functions
from ..stdlib.reflection import register_reflection_functions
from ..stdlib.fs_watch import register_fs_watch_functions
from ..stdlib.platform_linux import register_platform_linux_functions
from ..stdlib.platform_windows import register_platform_windows_functions
from ..stdlib.platform_macos import register_platform_macos_functions
from ..stdlib.gui import register_gui_functions


logger = logging.getLogger(__name__)


def _register_optional_graphics(runtime: Runtime) -> None:
    """Register optional graphics support when dependencies are available."""
    try:
        register_graphics_functions(runtime)
    except Exception as exc:
        # Keep stdlib boot resilient on systems without graphics dependencies,
        # but surface the reason for troubleshooting in production logs.
        logger.warning("Skipping optional graphics registration: %s", exc)


_STDLIB_REGISTRARS = (
    register_math_functions,
    register_string_functions,
    register_io_functions,
    register_system_functions,
    register_collections_functions,
    register_network_functions,
    register_stringbuilder_functions,
    register_iterator_functions,
    register_option_result_functions,
    register_type_functions,
    register_async_functions,
    register_ffi_functions,
    _register_optional_graphics,
    register_asm_functions,
    register_testing_functions,
    register_benchmark_functions,
    register_module_functions,
    register_filesystem_functions,
    register_math3d_functions,
    register_json_functions,
    register_regex_functions,
    register_datetime_functions,
    register_http_functions,
    register_crypto_functions,
    register_uuid_functions,
    register_random_functions,
    register_compression_functions,
    register_env_functions,
    register_subprocess_functions,
    register_camera_functions,
    register_shader_functions,
    register_mesh_functions,
    register_scene_functions,
    register_logging_functions,
    register_csv_functions,
    register_path_functions,
    register_signal_functions,
    register_argparse_functions,
    register_config_functions,
    register_file_io_functions,
    register_sqlite_functions,
    register_xml_functions,
    register_email_functions,
    register_template_functions,
    register_threading_functions,
    register_serialization_functions,
    register_websocket_functions,
    register_database_functions,
    register_pdf_functions,
    register_image_functions,
    register_validation_functions,
    register_cache_functions,
    register_statistics_functions,
    register_business_functions,
    register_data_functions,
    register_scientific_functions,
    register_linalg_functions,
    register_numint_functions,
    register_dsp_functions,
    register_plot_functions,
    register_audio_functions,
    register_result_utils_functions,
    register_property_testing_functions,
    register_coverage_utils_functions,
    register_bit_ops_functions,
    register_ctype_functions,
    register_limits_functions,
    register_algorithms_functions,
    register_errno_functions,
    register_simd_functions,
    register_interrupt_functions,
    register_type_trait_functions,
    register_hardware_functions,
    register_atomics_functions,
    register_allocator_functions,
    register_smart_pointer_functions,
    register_native_threading_functions,
    register_sync_functions,
    register_parallel_functions,
    register_kernel_functions,
    register_driver_functions,
    register_reflection_functions,
    register_fs_watch_functions,
    register_platform_linux_functions,
    register_platform_windows_functions,
    register_platform_macos_functions,
    register_gui_functions,
)


_STDLIB_MODULES = (
    "math",
    "string",
    "io",
    "system",
    "collections",
    "network",
    "types",
    "asyncio",
    "ffi",
    "asm",
    "testing",
    "modules",
    "filesystem",
    "json",
    "regex",
    "datetime",
    "http",
    "crypto",
    "uuid",
    "random",
    "compression",
    "env",
    "subprocess",
    "math3d",
    "camera",
    "shaders",
    "mesh",
    "scene",
    "logging",
    "csv",
    "path",
    "signal",
    "argparse",
    "config",
    "file_io",
    "files",
    "sqlite",
    "database",
    "xml",
    "email",
    "smtp",
    "templates",
    "hardware",
    "port_io",
    "atomics",
    "atomic",
    "allocators",
    "allocator",
    "smart_pointers",
    "smart_pointer",
    "threading",
    "threads",
    "sync",
    "synchronization",
    "business",
    "data",
    "scientific",
    "linalg",
    "linear_algebra",
    "numint",
    "numerical",
    "dsp",
    "signal_proc",
    "fft",
    "plot",
    "visualization",
    "charts",
    "audio",
    "audio_utils",
    "result_utils",
    "error_handling",
    "property_testing",
    "prop_test",
    "coverage_utils",
    "coverage",
    "reflection",
    "platform_linux",
    "platform_unix",
    "platform_windows",
    "platform_win32",
    "platform_macos",
    "platform_darwin",
    "gui",
    "windowing",
    "font",
)

def register_stdlib(runtime: Runtime) -> None:
    """Register all standard library functions with the runtime."""
    for registrar in _STDLIB_REGISTRARS:
        registrar(runtime)

    for module_name in _STDLIB_MODULES:
        runtime.register_module(module_name)
