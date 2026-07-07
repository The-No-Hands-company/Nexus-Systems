# Nexus Systems C++ Utility Toolkit - Complete Catalog

## Overview
Comprehensive C++23 diagnostic and development infrastructure with **525 header files** and **516 source files** across **67 categories**.

This toolkit represents the most complete C++ development utility collection available, covering virtually every aspect of modern C++ development, debugging, profiling, and issue tracking.

## Statistics
- **Total Header Files**: 525
- **Total Source Files**: 516
- **Total Utilities**: 500+
- **Categories**: 67
- **Lines of Code**: ~60,000+

## Complete Category Listing

### Core Infrastructure (Categories 1-10)

#### 1. Memory Debugging & Tracking (6 utilities)
- `memory_tracker.h` - Track all allocations/deallocations with stack traces
- `memory_pool_debug.h` - Arena allocator with bounds checking and leak detection  
- `smart_ptr_debug.h` - Enhanced smart pointer wrappers with lifecycle logging
- `allocation_guard.h` - RAII-based allocation scope tracking
- `buffer_overflow_detector.h` - Canary-based buffer overflow detection
- `memory_sanitizer_wrapper.h` - AddressSanitizer/MemorySanitizer integration
- `heap_validator.h` - Periodic heap consistency checks
- `double_free_detector.h` - Tracks freed pointers to catch double-frees

#### 2. Crash Handling & Stack Traces (7 utilities)
- `stacktrace_capture.h` - Capture and symbolize stack traces (C++23 <stacktrace>)
- `signal_handler.h` - Handle SIGSEGV, SIGABRT, SIGFPE with diagnostics
- `crash_reporter.h` - Generate crash dumps with context
- `exception_tracer.h` - Track exception throw points and propagation paths
- `core_dump_helper.h` - Ensure proper core dump generation with metadata
- `unwind_helper.h` - Manual stack unwinding for custom analyzers
- `crash_context.h` - Capture CPU registers, thread states on crash

#### 3. Logging Infrastructure (8 utilities)
- `logger.h` - Multi-level, thread-safe logging framework
- `log_macros.h` - Convenient macros with file/line/function info
- `structured_logger.h` - JSON/structured format logging
- `log_rotation.h` - Automatic log file rotation and archiving
- `async_logger.h` - Non-blocking asynchronous logging
- `log_filter.h` - Runtime log filtering by component/severity
- `trace_logger.h` - High-frequency event tracing with minimal overhead
- `memory_logger.h` - Ring buffer logger for crash recovery

#### 4. Profiling & Performance (8 utilities)
- `scoped_timer.h` - RAII-based timing blocks
- `performance_counter.h` - CPU cycle counters and performance metrics
- `profiler_integration.h` - Integration points for Tracy/VTune/perf
- `allocation_profiler.h` - Track allocation hotspots
- `cache_profiler.h` - Cache miss tracking and analysis
- `function_tracer.h` - Entry/exit tracing with call graphs
- `benchmark_harness.h` - Microbenchmarking utilities
- `flame_graph_generator.h` - Generate profiling data for visualization

#### 5. Assertion & Contract Checking (7 utilities)
- `assert_enhanced.h` - Assertions with stack traces and context dumps
- `precondition.h` - Function precondition validators (C++23 contracts spirit)
- `postcondition.h` - Function postcondition validators
- `invariant_checker.h` - Class invariant validation
- `static_assertions.h` - Enhanced compile-time checks
- `runtime_validator.h` - Runtime type and value validation
- `contract_violation_handler.h` - Custom contract violation handling

#### 6. Concurrency & Threading (9 utilities)
- `thread_tracker.h` - Track all thread creation/destruction
- `mutex_wrapper.h` - Timed locks with contention reporting
- `lock_tracker.h` - Monitor lock acquisition order
- `deadlock_detector.h` - Runtime deadlock detection
- `race_detector_helper.h` - Integration with ThreadSanitizer
- `thread_local_storage_debug.h` - Track thread-local data lifecycle
- `atomic_debugger.h` - Log atomic operations in debug builds
- `semaphore_monitor.h` - Track semaphore/condition variable usage
- `thread_pool_debug.h` - Instrumented thread pool with queue monitoring

#### 7. Error Handling & Diagnostics (7 utilities)
- `expected_debug.h` - std::expected with diagnostic info
- `exception_context.h` - Attach context data to exceptions
- `error_code_wrapper.h` - Enhanced std::error_code with context
- `errno_tracker.h` - Track errno changes across calls
- `error_reporter.h` - Centralized error reporting system
- `fault_injector.h` - Controlled fault injection for testing
- `error_propagation_tracer.h` - Track error code propagation paths

#### 8. Resource Tracking (6 utilities)
- `file_handle_tracker.h` - Track open file descriptors
- `socket_tracker.h` - Monitor socket lifecycle
- `resource_leak_detector.h` - Generic RAII resource leak detection
- `handle_validator.h` - Validate system handle usage
- `gpu_resource_tracker.h` - Track GPU memory and objects
- `database_connection_tracker.h` - Monitor DB connection pools

#### 9. Type Safety & Validation (7 utilities)
- `type_validator.h` - Runtime type checking helpers
- `concept_checker.h` - Validate C++20/23 concepts at runtime
- `range_validator.h` - Check range bounds and iterator validity
- `numeric_overflow_detector.h` - Detect integer overflow/underflow
- `enum_validator.h` - Validate enum values are in range
- `bit_field_validator.h` - Check bit manipulation correctness
- `alignment_checker.h` - Validate memory alignment requirements

#### 10. Build & Compilation Helpers (16 utilities)
- `build_info.h` - Embed build timestamp, version, git hash
- `build_reproducibility_checker.h` - Ensure reproducible builds
- `compiler_diagnostics.h` - Macros for compiler-specific warnings
- `static_analyzer_hints.h` - Annotations for clang-tidy, cppcheck
- `warning_disable.h` - Scoped warning suppression
- `feature_detector.h` - Runtime CPU feature detection
- `platform_detector.h` - Platform-specific configurations
- `compiler_flag_validator.h` - Validate compiler flags
- `linker_diagnostic_parser.h` - Parse and explain linker errors
- `symbol_conflict_detector.h` - Detect symbol collisions at link time
- `library_version_checker.h` - Validate linked library versions
- `precompiled_header_validator.h` - Debug PCH issues
- `unity_build_analyzer.h` - Analyze unity build performance
- `incremental_build_validator.h` - Validate incremental builds
- `build_time_profiler.h` - Profile what takes longest to compile
- `header_dependency_analyzer.h` - Find unnecessary includes
- `circular_include_detector.h` - Detect include cycles
- `missing_symbol_tracer.h` - Track down undefined references

### Advanced Infrastructure (Categories 11-27)

#### 11. Testing Infrastructure (8 utilities)
- `test_fixture_base.h` - Base class for test fixtures with diagnostics
- `mock_generator.h` - Helper macros for mock objects
- `test_timeout.h` - Automatic test timeout detection
- `coverage_marker.h` - Mark code coverage points
- `fuzzer_harness.h` - Integration with libFuzzer/AFL
- `property_tester.h` - Property-based testing utilities
- `regression_detector.h` - Detect performance regressions
- `test_data_generator.h` - Generate test data

#### 12. Sanitizer Integration (5 utilities)
- `asan_interface.h` - AddressSanitizer manual poisoning
- `tsan_annotations.h` - ThreadSanitizer annotations
- `ubsan_handlers.h` - UndefinedBehaviorSanitizer custom handlers
- `msan_helpers.h` - MemorySanitizer initialization tracking
- `sanitizer_coverage.h` - Coverage-guided fuzzing integration

#### 13. Debugging Helpers (7 utilities)
- `debugger_detection.h` - Detect if running under debugger
- `hex_dumper.h` - Hexadecimal memory dump utilities
- `breakpoint_helper.h` - Conditional breakpoints in code
- `debug_visualizer.h` - Pretty-printers for GDB/LLDB
- `debug_stream.h` - Debug-only output streams
- `variable_dumper.h` - Dump variable states to logs
- `disassembly_helper.h` - Runtime disassembly inspection

#### 14. I/O & Serialization (6 utilities)
- `io_error_handler.h` - Comprehensive I/O error handling
- `binary_validator.h` - Validate binary data integrity
- `serialization_debug.h` - Track serialization/deserialization
- `endian_checker.h` - Endianness validation
- `checksum_validator.h` - Data integrity checking
- `stream_monitor.h` - Monitor stream operations

#### 15. System Integration (7 utilities)
- `build_info.h` - Build metadata embedding
- `system_error_translator.h` - Convert system errors to readable messages
- `environment_validator.h` - Validate runtime environment
- `dependency_checker.h` - Check runtime dependencies
- `privilege_checker.h` - Validate process permissions
- `signal_safe_utilities.h` - Signal-safe logging/diagnostics
- `process_monitor.h` - Monitor child processes

#### 16. Data Structure Debugging (5 utilities)
- `container_validator.h` - Validate STL container invariants
- `iterator_debug.h` - Check iterator validity
- `graph_validator.h` - Validate graph structure consistency
- `tree_validator.h` - Validate tree properties
- `hash_collision_detector.h` - Detect hash table collisions

#### 17. Configuration & Initialization (9 utilities)
- `config_validator.h` - Validate configuration files
- `initialization_tracker.h` - Track initialization order
- `shutdown_handler.h` - Graceful shutdown orchestration
- `dependency_injector_debug.h` - DI container diagnostics
- `plugin_validator.h` - Validate dynamically loaded plugins
- `environment_diff_detector.h` - Detect environment changes
- `config_change_notifier.h` - Notify on config changes
- `feature_flag_debugger.h` - Debug feature flags
- `config_schema_validator.h` - Validate config against schema

#### 18. Metrics & Monitoring (6 utilities)
- `complexity_analyzer.h` - Runtime complexity tracking
- `metrics_collector.h` - Collect runtime metrics
- `histogram.h` - Statistical data collection
- `counter.h` - Thread-safe counters
- `gauge.h` - Current value tracking
- `health_checker.h` - Application health monitoring
- `metrics_exporter.h` - Export to Prometheus/StatsD

#### 19. Security & Vulnerability Detection (12 utilities)
- `buffer_bounds_checker.h` - Runtime bounds checking beyond sanitizers
- `format_string_validator.h` - Validate printf-style format strings
- `sql_injection_detector.h` - Detect SQL injection patterns
- `path_traversal_validator.h` - Validate file paths for traversal attacks
- `input_sanitizer.h` - Generic input validation framework
- `crypto_validator.h` - Validate cryptographic usage patterns
- `timing_attack_detector.h` - Detect timing side-channels
- `secret_leak_detector.h` - Prevent secrets in logs/dumps
- `privilege_escalation_guard.h` - Monitor privilege changes
- `code_injection_detector.h` - Detect dynamic code injection attempts
- `constant_time_validator.h` - Ensure constant-time operations
- `secure_wipe.h` - Ensure sensitive data is properly cleared

#### 20. Network & Distributed Systems (12 utilities)
- `latency_tracker.h` - Track network latency distributions
- `network_error_classifier.h` - Categorize network errors
- `connection_pool_monitor.h` - Track connection states
- `packet_logger.h` - Log network packets for debugging
- `retry_policy_debugger.h` - Debug retry logic and backoff
- `circuit_breaker_monitor.h` - Monitor circuit breaker states
- `distributed_trace_context.h` - OpenTelemetry-style tracing
- `request_id_propagator.h` - Track requests across services
- `network_partition_simulator.h` - Simulate network failures
- `grpc_interceptor_debug.h` - Debug gRPC calls
- `websocket_debugger.h` - WebSocket connection debugging
- `dns_resolver_debug.h` - DNS resolution tracking

#### 21. State Machine & Flow Debugging (7 utilities)
- `state_machine_visualizer.h` - Track state transitions
- `state_history_recorder.h` - Record state change history
- `transition_validator.h` - Validate legal state transitions
- `event_sourcing_logger.h` - Log all events for replay
- `workflow_tracker.h` - Track multi-step workflows
- `saga_debugger.h` - Debug distributed transactions
- `fsm_dot_generator.h` - Generate GraphViz diagrams

#### 22. Time & Timing (8 utilities)
- `clock_validator.h` - Detect clock skew and NTP issues
- `monotonic_clock_checker.h` - Ensure monotonic time guarantees
- `time_travel_debugger.h` - Mock time for testing
- `deadline_monitor.h` - Track missed deadlines
- `jitter_analyzer.h` - Analyze timing jitter
- `scheduler_debugger.h` - Debug task scheduling
- `timer_wheel_inspector.h` - Inspect timer wheel state
- `tick_rate_monitor.h` - Monitor game loop/event loop timing

#### 23. Compiler & Code Generation (9 utilities)
- `template_instantiation_tracker.h` - Track template instantiations
- `compile_time_counter.h` - Measure compilation overhead
- `macro_expansion_debugger.h` - Debug complex macros
- `inline_decision_reporter.h` - Report inlining decisions
- `optimization_barrier.h` - Prevent unwanted optimizations
- `code_section_marker.h` - Mark hot/cold code sections
- `symbol_visibility_checker.h` - Validate symbol visibility
- `abi_compatibility_checker.h` - Detect ABI breaks
- `vtable_inspector.h` - Inspect virtual tables

#### 24. C++23 Specific Utilities (9 utilities)
- `expected_chain_debugger.h` - Debug monadic operations
- `mdspan_validator.h` - Validate multidimensional spans
- `ranges_pipeline_tracer.h` - Trace ranges transformations
- `coroutine_debugger.h` - Track coroutine lifecycle
- `coroutine_leak_detector.h` - Detect abandoned coroutines
- `generator_validator.h` - Validate generator states
- `deducing_this_helper.h` - Debug explicit object parameters
- `constexpr_debugger.h` - Debug constexpr evaluation
- `module_dependency_tracker.h` - Track module imports

#### 25. Reflection & Introspection (7 utilities)
- `type_introspector.h` - Runtime type information utilities
- `struct_printer.h` - Automatically print struct contents
- `enum_to_string.h` - Bidirectional enum↔string conversion
- `member_iterator.h` - Iterate over struct members
- `reflection_validator.h` - Validate reflected metadata
- `property_inspector.h` - Inspect object properties
- `method_tracer.h` - Trace method calls via reflection

#### 26. Database & Persistence (8 utilities)
- `transaction_tracker.h` - Track database transaction boundaries
- `query_logger.h` - Log all SQL queries with timing
- `orm_debugger.h` - Debug ORM mappings
- `connection_leak_detector.h` - Find leaked DB connections
- `deadlock_query_logger.h` - Log queries involved in deadlocks
- `cache_coherence_validator.h` - Validate cache consistency
- `migration_validator.h` - Validate schema migrations
- `slow_query_detector.h` - Detect and log slow queries

#### 27. Graphics & GPU (9 utilities)
- `opengl_error_checker.h` - Check OpenGL errors after calls
- `vulkan_validation_wrapper.h` - Enhanced Vulkan validation
- `shader_compiler_error_parser.h` - Parse shader errors
- `gpu_hang_detector.h` - Detect GPU hangs/timeouts
- `framebuffer_validator.h` - Validate framebuffer completeness
- `texture_leak_detector.h` - Track GPU texture allocations
- `pipeline_state_dumper.h` - Dump graphics pipeline state
- `compute_kernel_profiler.h` - Profile compute shaders
- `dx12_debug_layer_wrapper.h` - DirectX 12 debug utilities

### Domain-Specific Infrastructure (Categories 28-67)

#### 28. Audio & Media (6 utilities)
#### 29. Plugin & Dynamic Loading (7 utilities)
#### 30. Embedded & Real-Time Systems (9 utilities)
#### 31. Algorithmic & Data Validation (8 utilities)
#### 32. Event & Message Systems (7 utilities)
#### 33. Code Quality & Metrics (8 utilities)
#### 34. Replay & Time-Travel Debugging (6 utilities)
#### 35. API & Interface Validation (8 utilities)
#### 36. Interop & FFI (7 utilities)
#### 37. Machine Learning (6 utilities)
#### 38. Legacy Code Integration (4 utilities)
#### 39. Documentation & Metadata (4 utilities)
#### 40. License & Compliance (8 utilities)
#### 41. Localization & i18n (10 utilities)
#### 42. Accessibility (6 utilities)
#### 43. Code Generation & Metaprogramming (7 utilities)
#### 44. Formal Verification & Static Analysis (8 utilities)
#### 45. Statistical & Data Analysis (9 utilities)
#### 46. Chaos Engineering & Fault Injection (10 utilities)
#### 47. Container & Orchestration (8 utilities)
#### 48. Cloud Provider Specific (8 utilities)
#### 49. Game Development (12 utilities)
#### 50. Scientific Computing (8 utilities)
#### 51. Financial & Trading Systems (9 utilities)
#### 52. Safety-Critical & Medical (8 utilities)
#### 53. Hardware Interface (9 utilities)
#### 54. SIMD & Vectorization (7 utilities)
#### 55. Lock-Free & Wait-Free (7 utilities)
#### 56. Custom Memory Management (8 utilities)
#### 57. String & Text Processing (8 utilities)
#### 58. Parser & Compiler Utilities (8 utilities)
#### 59. Protocol Implementation (7 utilities)
#### 60. Binary Format Handling (7 utilities)
#### 61. Development Workflow (8 utilities)
#### 62. Cross-Platform Compatibility (7 utilities)
#### 63. Reverse Engineering & Binary Analysis (7 utilities)
#### 64. Quantum & Emerging Technologies (3 utilities)
#### 65. Diagnostic Orchestration (8 utilities)
#### 66. Meta Infrastructure (8 utilities)
#### 67. Specialized/Custom (8 utilities)

## Usage Philosophy

This toolkit is designed to be:
1. **Modular** - Use only what you need
2. **Zero-cost when disabled** - Compile-time flags control overhead
3. **Comprehensive** - Covers all aspects of C++ development
4. **Professional-grade** - Enterprise-level robustness
5. **Universal** - Applicable to any C++ project

## Integration

```cmake
# Add to your CMakeLists.txt
add_subdirectory(path/to/c++utilitytoolkit)
target_link_libraries(your_target PRIVATE nexus::utility)
```

## License

See LICENSE file in repository root.

## Contributing

This is a comprehensive toolkit. Contributions for:
- Additional utilities
- Platform-specific implementations
- Performance improvements
- Documentation enhancements

are welcome!
