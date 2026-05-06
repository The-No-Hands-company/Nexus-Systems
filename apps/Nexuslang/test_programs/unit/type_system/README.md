# Type System Test Programs

This directory contains comprehensive test programs for NLPL's type system.

## Test Organization

### Type Inference Tests (`test_type_inference.nlpl`)
Tests bidirectional type inference, lambda inference, and expression type inference:
- Basic type inference from literals
- List and dictionary type inference
- Function return type inference
- Lambda parameter type inference from context
- Bidirectional inference with expected types
- Method chain type inference
- Nested call type inference
- Union type inference
- Contextual inference for empty collections

### Generic Types Tests (`test_generic_types.nlpl`)
Tests generic type instantiation, type parameter inference, and constraints:
- Generic list and dictionary instantiation
- Generic functions with type parameters
- Multiple type parameters (e.g., `map<T, R>`)
- Type parameter inference from arguments
- Nested generic types (`List<List<T>>`)
- Generic type constraints
- Variance testing (covariant/contravariant)

### User-Defined Types Tests (`test_user_defined_types.nlpl`)
Tests class types, inheritance, and property/method type checking:
- Basic classes with typed properties
- Class inheritance
- Property and method type checking
- Multiple inheritance levels
- Polymorphism
- Subtype compatibility
- Abstract classes
- Interface-like behavior

### Type Compatibility Tests (`test_type_compatibility.nlpl`)
Tests type compatibility checking, subtyping, and type conversions:
- Basic type compatibility
- Numeric type widening (Integer to Float)
- String concatenation compatibility
- List element type compatibility
- Function parameter compatibility
- Return type compatibility
- Union type compatibility
- Class inheritance compatibility
- Lambda parameter compatibility
- Collection element compatibility

## Running Tests

To run all type system tests:
```bash
python src/main.py test_programs/unit/type_system/test_type_inference.nlpl
python src/main.py test_programs/unit/type_system/test_generic_types.nlpl
python src/main.py test_programs/unit/type_system/test_user_defined_types.nlpl
python src/main.py test_programs/unit/type_system/test_type_compatibility.nlpl
```

To run with type checking enabled:
```bash
python src/main.py test_programs/unit/type_system/test_type_inference.nlpl --type-check
```

To run with debug output:
```bash
python src/main.py test_programs/unit/type_system/test_type_inference.nlpl --debug
```

## Test Coverage

These tests cover:
- Basic type inference (literals, expressions)
- Bidirectional type inference (lambdas, expected types)
- Generic type instantiation (List<T>, Dictionary<K, V>)
- Type parameter inference (automatic type argument deduction)
- User-defined class types
- Inheritance and subtyping
- Type compatibility checking
- Method and property type checking
- Lambda type inference
- Nested generic types
- Generic constraints (partially - awaiting full implementation)
- Variance annotations (conceptual tests)

## Expected Behavior

**Successful tests** should:
1. Complete without runtime errors
2. Print "All [test type] tests completed!" at the end
3. Demonstrate correct type inference behavior
4. Show proper type checking and compatibility

**Note**: Some test programs intentionally contain syntax errors to test edge cases. This is expected and documented in the code comments.

## Integration with Type System

These tests validate:
- `TypeInferenceEngine` - Bidirectional inference, lambda inference
- `GenericTypeRegistry` - Generic type instantiation
- `GenericTypeInference` - Type parameter inference
- `TypeRegistry` - User-defined types with inheritance
- `IntegratedTypeSystem` - Complete type system integration

## Future Tests

Planned additions:
- [ ] Trait system tests
- [ ] Advanced constraint tests (multiple bounds)
- [ ] Variance annotation tests (in/out modifiers)
- [ ] Type alias tests
- [ ] Recursive type tests
- [ ] Dependent type tests (if implemented)
