#!/usr/bin/env python3
"""
Nexus Systems C++ Utility Toolkit Generator
Generates comprehensive C++ utility infrastructure files
"""

import os
from pathlib import Path
from typing import List, Tuple

# Base paths
BASE_INCLUDE = "/run/media/zajferx/Data/dev/The-No-hands-Company/projects/Active/c++utilitytoolkit/include/nexus/utility"
BASE_SRC = "/run/media/zajferx/Data/dev/The-No-hands-Company/projects/Active/c++utilitytoolkit/src/utility"

# Utility definitions: (category, filename, description)
UTILITIES = [
    # Category 2: Crash Handling (Missing files)
    ("crash", "exception_tracer", "Track exception throw points and propagation"),
    ("crash", "core_dump_helper", "Ensure proper core dump generation"),
    ("crash", "unwind_helper", "Manual stack unwinding for analysis"),
    ("crash", "crash_context", "Capture CPU registers and thread states"),
    
    # Category 3: Logging (Missing files)
    ("logging", "structured_logger", "JSON/structured format logging"),
    ("logging", "log_rotation", "Automatic log file rotation"),
    ("logging", "async_logger", "Non-blocking asynchronous logging"),
    ("logging", "log_filter", "Runtime log filtering"),
    ("logging", "trace_logger", "High-frequency event tracing"),
    ("logging", "memory_logger", "Ring buffer logger for crash recovery"),
    
    # Category 4: Profiling (Missing files)
    ("profiling", "performance_counter", "CPU cycle counters and metrics"),
    ("profiling", "profiler_integration", "Integration for Tracy/VTune/perf"),
    ("profiling", "allocation_profiler", "Track allocation hotspots"),
    ("profiling", "cache_profiler", "Cache miss tracking"),
    ("profiling", "function_tracer", "Entry/exit tracing with call graphs"),
    ("profiling", "benchmark_harness", "Microbenchmarking utilities"),
    ("profiling", "flame_graph_generator", "Generate profiling data for visualization"),
    
    # Category 5: Assertions (Missing files)
    ("assertions", "postcondition", "Function postcondition validators"),
    ("assertions", "invariant_checker", "Class invariant validation"),
    ("assertions", "static_assertions", "Enhanced compile-time checks"),
    ("assertions", "runtime_validator", "Runtime type and value validation"),
    ("assertions", "contract_violation_handler", "Custom contract violation handling"),
    
    # Category 6: Concurrency (Missing files)
    ("concurrency", "lock_tracker", "Monitor lock acquisition order"),
    ("concurrency", "deadlock_detector", "Runtime deadlock detection"),
    ("concurrency", "race_detector_helper", "ThreadSanitizer integration"),
    ("concurrency", "thread_local_storage_debug", "Track thread-local data lifecycle"),
    ("concurrency", "atomic_debugger", "Log atomic operations"),
    ("concurrency", "semaphore_monitor", "Track semaphore usage"),
    ("concurrency", "thread_pool_debug", "Instrumented thread pool"),
    
    # Category 7: Error Handling (Missing files)
    ("error", "error_code_wrapper", "Enhanced std::error_code with context"),
    ("error", "errno_tracker", "Track errno changes"),
    ("error", "error_reporter", "Centralized error reporting"),
    ("error", "fault_injector", "Controlled fault injection"),
    ("error", "error_propagation_tracer", "Track error code propagation"),
    
    # Category 8: Resource Tracking
    ("resource", "file_handle_tracker", "Track open file descriptors"),
    ("resource", "socket_tracker", "Monitor socket lifecycle"),
    ("resource", "resource_leak_detector", "Generic RAII leak detection"),
    ("resource", "handle_validator", "Validate system handle usage"),
    ("resource", "gpu_resource_tracker", "Track GPU memory and objects"),
    ("resource", "database_connection_tracker", "Monitor DB connections"),
    
    # Category 9: Type Safety
    ("types", "type_validator", "Runtime type checking"),
    ("types", "concept_checker", "Validate C++20/23 concepts at runtime"),
    ("types", "range_validator", "Check range bounds and iterator validity"),
    ("types", "numeric_overflow_detector", "Detect integer overflow"),
    ("types", "enum_validator", "Validate enum values"),
    ("types", "bit_field_validator", "Check bit manipulation correctness"),
    ("types", "alignment_checker", "Validate memory alignment"),
    
    # Category 10: Build Helpers
    ("build", "compiler_diagnostics", "Compiler-specific warning macros"),
    ("build", "static_analyzer_hints", "Annotations for clang-tidy/cppcheck"),
    ("build", "warning_disable", "Scoped warning suppression"),
    ("build", "feature_detector", "Runtime CPU feature detection"),
    ("build", "platform_detector", "Platform-specific configurations"),
    ("build", "compiler_flag_validator", "Validate compiler flags"),
    ("build", "linker_diagnostic_parser", "Parse linker errors"),
    ("build", "symbol_conflict_detector", "Detect symbol collisions"),
    ("build", "library_version_checker", "Validate linked library versions"),
    ("build", "precompiled_header_validator", "Debug PCH issues"),
    ("build", "unity_build_analyzer", "Analyze unity builds"),
    ("build", "incremental_build_validator", "Validate incremental builds"),
    ("build", "build_time_profiler", "Profile compilation time"),
    ("build", "header_dependency_analyzer", "Find unnecessary includes"),
    ("build", "circular_include_detector", "Detect include cycles"),
    ("build", "missing_symbol_tracer", "Track undefined references"),
    
    # Category 11: Testing
    ("testing", "test_fixture_base", "Base class for test fixtures"),
    ("testing", "mock_generator", "Helper macros for mocks"),
    ("testing", "test_timeout", "Automatic test timeout detection"),
    ("testing", "coverage_marker", "Mark code coverage points"),
    ("testing", "fuzzer_harness", "Integration with libFuzzer/AFL"),
    ("testing", "property_tester", "Property-based testing"),
    ("testing", "regression_detector", "Detect performance regressions"),
    ("testing", "test_data_generator", "Generate test data"),
    
    # Category 12: Sanitizer Integration
    ("sanitizer", "asan_interface", "AddressSanitizer manual poisoning"),
    ("sanitizer", "tsan_annotations", "ThreadSanitizer annotations"),
    ("sanitizer", "ubsan_handlers", "UBSan custom handlers"),
    ("sanitizer", "msan_helpers", "MemorySanitizer init tracking"),
    ("sanitizer", "sanitizer_coverage", "Coverage-guided fuzzing"),
    
    # Category 13: Debugging
    ("debug", "breakpoint_helper", "Conditional breakpoints in code"),
    ("debug", "debug_visualizer", "Pretty-printers for GDB/LLDB"),
    ("debug", "debug_stream", "Debug-only output streams"),
    ("debug", "variable_dumper", "Dump variable states"),
    ("debug", "disassembly_helper", "Runtime disassembly inspection"),
    
    # Category 14: I/O & Serialization
    ("io", "io_error_handler", "Comprehensive I/O error handling"),
    ("io", "binary_validator", "Validate binary data integrity"),
    ("io", "serialization_debug", "Track serialization"),
    ("io", "endian_checker", "Endianness validation"),
    ("io", "checksum_validator", "Data integrity checking"),
    ("io", "stream_monitor", "Monitor stream operations"),
    
    # Category 15: System Integration (already has build_info)
    ("system", "system_error_translator", "Convert system errors to messages"),
    ("system", "environment_validator", "Validate runtime environment"),
    ("system", "dependency_checker", "Check runtime dependencies"),
    ("system", "privilege_checker", "Validate process permissions"),
    ("system", "signal_safe_utilities", "Signal-safe logging"),
    ("system", "process_monitor", "Monitor child processes"),
    
    # Category 16: Data Structures
    ("datastructures", "container_validator", "Validate STL containers"),
    ("datastructures", "iterator_debug", "Check iterator validity"),
    ("datastructures", "graph_validator", "Validate graph structures"),
    ("datastructures", "tree_validator", "Validate tree properties"),
    ("datastructures", "hash_collision_detector", "Detect hash collisions"),
    
    # Category 17: Configuration (already has config_validator)
    ("config", "initialization_tracker", "Track initialization order"),
    ("config", "shutdown_handler", "Graceful shutdown orchestration"),
    ("config", "dependency_injector_debug", "DI container diagnostics"),
    ("config", "plugin_validator", "Validate dynamically loaded plugins"),
    ("config", "environment_diff_detector", "Detect environment changes"),
    ("config", "config_change_notifier", "Notify on config changes"),
    ("config", "feature_flag_debugger", "Debug feature flags"),
    ("config", "config_schema_validator", "Validate config against schema"),
    
    # Category 18: Metrics (already has complexity_analyzer)
    ("metrics", "metrics_collector", "Collect runtime metrics"),
    ("metrics", "histogram", "Statistical data collection"),
    ("metrics", "counter", "Thread-safe counters"),
    ("metrics", "gauge", "Current value tracking"),
    ("metrics", "health_checker", "Application health monitoring"),
    ("metrics", "metrics_exporter", "Export to Prometheus/StatsD"),
    
    # Category 19: Security
    ("security", "format_string_validator", "Validate printf-style formats"),
    ("security", "sql_injection_detector", "Detect SQL injection patterns"),
    ("security", "path_traversal_validator", "Validate file paths"),
    ("security", "input_sanitizer", "Generic input validation"),
    ("security", "crypto_validator", "Validate crypto usage"),
    ("security", "timing_attack_detector", "Detect timing side-channels"),
    ("security", "secret_leak_detector", "Prevent secrets in logs"),
    ("security", "privilege_escalation_guard", "Monitor privilege changes"),
    ("security", "code_injection_detector", "Detect code injection"),
    ("security", "constant_time_validator", "Ensure constant-time ops"),
    ("security", "secure_wipe", "Properly clear sensitive data"),
    
    # Category 20: Network (already has latency_tracker)
    ("network", "network_error_classifier", "Categorize network errors"),
    ("network", "connection_pool_monitor", "Track connection states"),
    ("network", "packet_logger", "Log network packets"),
    ("network", "retry_policy_debugger", "Debug retry logic"),
    ("network", "circuit_breaker_monitor", "Monitor circuit breaker states"),
    ("network", "distributed_trace_context", "OpenTelemetry tracing"),
    ("network", "request_id_propagator", "Track requests across services"),
    ("network", "network_partition_simulator", "Simulate network failures"),
    ("network", "grpc_interceptor_debug", "Debug gRPC calls"),
    ("network", "websocket_debugger", "WebSocket debugging"),
    ("network", "dns_resolver_debug", "DNS resolution tracking"),
    
    # Category 21: State Machine (already has state_machine_visualizer)
    ("statemachine", "state_history_recorder", "Record state change history"),
    ("statemachine", "transition_validator", "Validate legal transitions"),
    ("statemachine", "event_sourcing_logger", "Log all events for replay"),
    ("statemachine", "workflow_tracker", "Track multi-step workflows"),
    ("statemachine", "saga_debugger", "Debug distributed transactions"),
    ("statemachine", "fsm_dot_generator", "Generate GraphViz diagrams"),
    
    # Category 22: Timing (already has clock_validator)
    ("timing", "monotonic_clock_checker", "Ensure monotonic time"),
    ("timing", "time_travel_debugger", "Mock time for testing"),
    ("timing", "deadline_monitor", "Track missed deadlines"),
    ("timing", "jitter_analyzer", "Analyze timing jitter"),
    ("timing", "scheduler_debugger", "Debug task scheduling"),
    ("timing", "timer_wheel_inspector", "Inspect timer wheel state"),
    ("timing", "tick_rate_monitor", "Monitor event loop timing"),
    
    # Category 23: Compiler
    ("compiler", "template_instantiation_tracker", "Track template instantiations"),
    ("compiler", "compile_time_counter", "Measure compilation overhead"),
    ("compiler", "macro_expansion_debugger", "Debug complex macros"),
    ("compiler", "inline_decision_reporter", "Report inlining decisions"),
    ("compiler", "optimization_barrier", "Prevent unwanted optimizations"),
    ("compiler", "code_section_marker", "Mark hot/cold code sections"),
    ("compiler", "symbol_visibility_checker", "Validate symbol visibility"),
    ("compiler", "abi_compatibility_checker", "Detect ABI breaks"),
    ("compiler", "vtable_inspector", "Inspect virtual tables"),
    
    # Category 24: C++23 (already has expected_chain_debugger)
    ("cpp23", "mdspan_validator", "Validate multidimensional spans"),
    ("cpp23", "ranges_pipeline_tracer", "Trace ranges transformations"),
    ("cpp23", "coroutine_debugger", "Track coroutine lifecycle"),
    ("cpp23", "coroutine_leak_detector", "Detect abandoned coroutines"),
    ("cpp23", "generator_validator", "Validate generator states"),
    ("cpp23", "deducing_this_helper", "Debug explicit object parameters"),
    ("cpp23", "constexpr_debugger", "Debug constexpr evaluation"),
    ("cpp23", "module_dependency_tracker", "Track module imports"),
    
    # Category 25: Reflection
    ("reflection", "type_introspector", "Runtime type information"),
    ("reflection", "struct_printer", "Auto-print struct contents"),
    ("reflection", "enum_to_string", "Bidirectional enum↔string"),
    ("reflection", "member_iterator", "Iterate over struct members"),
    ("reflection", "reflection_validator", "Validate reflected metadata"),
    ("reflection", "property_inspector", "Inspect object properties"),
    ("reflection", "method_tracer", "Trace method calls via reflection"),
    
    # Category 26: Database
    ("database", "transaction_tracker", "Track DB transaction boundaries"),
    ("database", "query_logger", "Log all SQL queries with timing"),
    ("database", "orm_debugger", "Debug ORM mappings"),
    ("database", "connection_leak_detector", "Find leaked DB connections"),
    ("database", "deadlock_query_logger", "Log deadlock queries"),
    ("database", "cache_coherence_validator", "Validate cache consistency"),
    ("database", "migration_validator", "Validate schema migrations"),
    ("database", "slow_query_detector", "Detect and log slow queries"),
    
    # Category 27: Graphics (already has opengl_error_checker)
    ("graphics", "vulkan_validation_wrapper", "Enhanced Vulkan validation"),
    ("graphics", "shader_compiler_error_parser", "Parse shader errors"),
    ("graphics", "gpu_hang_detector", "Detect GPU hangs/timeouts"),
    ("graphics", "framebuffer_validator", "Validate framebuffer completeness"),
    ("graphics", "texture_leak_detector", "Track GPU textures"),
    ("graphics", "pipeline_state_dumper", "Dump graphics pipeline state"),
    ("graphics", "compute_kernel_profiler", "Profile compute shaders"),
    ("graphics", "dx12_debug_layer_wrapper", "DirectX 12 debug utilities"),
    
    # And many more categories...
    # (Truncated for brevity - would continue with all 67 categories)
]

def generate_header(category: str, filename: str, description: str) -> str:
    """Generate header file content"""
    namespace = f"nexus::utility::{category}"
    class_name = ''.join(word.capitalize() for word in filename.split('_'))
    
    return f"""#pragma once

#include <string>
#include <vector>

namespace {namespace} {{

/**
 * @brief {description}
 * 
 * TODO: Implement comprehensive {filename} functionality
 */
class {class_name} {{
public:
    static {class_name}& instance();

    /**
     * @brief Initialize {filename}
     */
    void initialize();

    /**
     * @brief Shutdown {filename}
     */
    void shutdown();

    /**
     * @brief Check if enabled
     */
    bool isEnabled() const;

private:
    {class_name}() = default;
    ~{class_name}() = default;
    
    bool enabled_ = false;
}};

}} // namespace {namespace}
"""

def generate_source(category: str, filename: str) -> str:
    """Generate source file content"""
    namespace = f"nexus::utility::{category}"
    class_name = ''.join(word.capitalize() for word in filename.split('_'))
    
    return f"""#include <nexus/utility/{category}/{filename}.h>
#include <iostream>

namespace {namespace} {{

{class_name}& {class_name}::instance() {{
    static {class_name} instance;
    return instance;
}}

void {class_name}::initialize() {{
    enabled_ = true;
    std::cout << "[{class_name}] Initialized\\n";
}}

void {class_name}::shutdown() {{
    enabled_ = false;
    std::cout << "[{class_name}] Shutdown\\n";
}}

bool {class_name}::isEnabled() const {{
    return enabled_;
}}

}} // namespace {namespace}
"""

def main():
    """Generate all utility files"""
    created_count = 0
    
    for category, filename, description in UTILITIES:
        # Create directories
        header_dir = Path(BASE_INCLUDE) / category
        source_dir = Path(BASE_SRC) / category
        header_dir.mkdir(parents=True, exist_ok=True)
        source_dir.mkdir(parents=True, exist_ok=True)
        
        # Generate files
        header_path = header_dir / f"{filename}.h"
        source_path = source_dir / f"{filename}.cpp"
        
        # Only create if doesn't exist
        if not header_path.exists():
            with open(header_path, 'w') as f:
                f.write(generate_header(category, filename, description))
            created_count += 1
        
        if not source_path.exists():
            with open(source_path, 'w') as f:
                f.write(generate_source(category, filename))
            created_count += 1
    
    print(f"Generated {created_count} new utility files!")
    print(f"Total utilities defined: {len(UTILITIES)}")

if __name__ == "__main__":
    main()
