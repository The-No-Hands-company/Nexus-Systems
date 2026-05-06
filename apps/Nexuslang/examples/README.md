# NexusLang Examples Directory

This directory contains example programs demonstrating NexusLang language features, organized by topic for progressive learning.

## Organization

### Core Basics (01-05)
Examples covering fundamental language concepts and syntax:

- **01_basic_concepts.nlpl** - Variables, control flow, basic operations
- **02_object_oriented.nlpl** - Classes, objects, inheritance
- **03_unified_syntax.nlpl** - Natural language syntax variations
- **04_type_system_basics.nlpl** - Basic type system features
- **05_typed_program.nlpl** - Complete typed program example

### OOP & Type System Depth (06-15)
Object-oriented and type-system depth topics:

- **06_polymorphic_type_system.nlpl** - Polymorphism and rich type constraints
- **07_type_system_features.nlpl** - Type system capabilities
- **08_type_features_index.nlpl** - Index to type-system examples (09-14)
- **09_generic_classes.nlpl** - Generic classes with type parameters
- **10_abstract_classes.nlpl** - Abstract classes and methods
- **11_traits.nlpl** - Trait definitions and implementations
- **12_type_aliases.nlpl** - Type aliases for complex types
- **13_type_guards.nlpl** - Runtime type checking
- **14_variance.nlpl** - Covariance, contravariance, invariance
- **15_design_patterns.nlpl** - Common design patterns

### Functional Programming (16-18)
Functional programming paradigms and patterns:

- **16_functional_programming.nlpl** - Functional programming concepts
- **17_lambda_and_higher_order_functions.nlpl** - Lambdas and HOFs
- **18_functional_programming_patterns.nlpl** - Functional patterns

### Concurrency & Parallelism (19-21)
Concurrent and event-driven programming:

- **19_concurrent_programming.nlpl** - Threads, locks, async
- **20_reactive_programming.nlpl** - Reactive patterns
- **21_event_driven_programming.nlpl** - Event handlers and listeners

### Low-Level Features (22-23)
Direct hardware and memory manipulation:

- **22_pointer_operations.nlpl** - Pointers, addresses, memory access
- **23_struct_and_union.nlpl** - C-style structs and unions

### Application Domains (24-26)
Practical application development:

- **24_network_programming.nlpl** - Network protocols, sockets
- **25_web_programming.nlpl** - Web applications and APIs
- **26_database_programming.nlpl** - Database operations

### Software Engineering Practices (27-31)
Professional development practices:

- **27_testing.nlpl** - Unit tests, integration tests
- **28_error_handling_and_logging.nlpl** - Error handling strategies
- **29_security_and_authentication.nlpl** - Security best practices
- **30_performance_optimization.nlpl** - Performance tuning
- **31_dependency_injection.nlpl** - DI patterns

### Feature Showcase (32)
- **32_feature_showcase.nlpl** - Comprehensive feature demonstration

### Standard Library Examples (Unnumbered)
Standard library usage demonstrations:

- **stdlib_complete.nlpl** - Complete stdlib overview
- **stdlib_demo.nlpl** - Interactive stdlib demo
- **stdlib_modules.nlpl** - Module-by-module examples

## Learning Path

### Beginner
Start with: 01 02 03 04 05

### Intermediate
Progress to: 06 07 09 10 11 16 17

### Advanced
Explore: 08 (index) 12-15 18-21 22-23

### Application Developer
Focus on: 24-31

### Complete Overview
Review: 32 (feature showcase)

## Running Examples

```bash
# Run any example
python -m nexuslang.main examples/01_basic_concepts.nlpl

# With debug output
python -m nexuslang.main examples/01_basic_concepts.nlpl --debug

# Without type checking
python -m nexuslang.main examples/01_basic_concepts.nlpl --no-type-check
```

## Contributing Examples

When adding new examples:
1. Choose appropriate number based on category
2. Use descriptive filename: `NN_topic_description.nlpl`
3. Include comments explaining concepts
4. Provide usage examples
5. Update this README
