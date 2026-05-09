# Trait/Interface Conformance Diagnostics

## Overview

Trait/Interface Conformance Diagnostics is a type system feature that provides detailed, actionable error messages when classes don't properly implement their declared traits or interfaces. Instead of generic "not implemented" errors, developers get clear diagnostics including:

- Which methods are missing
- Which methods have incompatible signatures
- What the trait requires
- Actionable suggestions for fixing the issues

## Features

### 1. Comprehensive Conformance Checking

The `get_trait_conformance_diagnostics()` method analyzes a class against a trait and returns:

```python
{
    'conforms': bool,                           # Does class fully implement trait?
    'missing_methods': List[str],              # Methods required but not defined
    'incompatible_methods': Dict[str, str],    # Methods with signature mismatches
    'suggestions': List[str],                  # Actionable fix recommendations
    'required_methods': List[str],             # All methods the trait requires
    'trait_name': str,                         # Name of the trait being checked
    'class_name': str,                         # Name of the class being checked
}
```

### 2. Method Signature Validation

Detects incompatibilities such as:
- **Parameter count mismatch**: Method expects N parameters, implementation has M
- **Return type mismatch**: Method returns type X, implementation returns type Y
- **Type incompatibility**: Parameter types don't match trait specification

Example diagnostic:
```
Method 'add' in class 'Vector' has signature mismatch with trait 'Numeric':
  Expected: add(Integer, Integer) -> Integer
  Got:      add(Integer) -> Integer
```

### 3. Intelligent Suggestions

Diagnostics include helpful suggestions for fixing issues:

```python
diagnostics['suggestions'] = [
    "Class 'Shape' is missing 1 method and has 1 incompatible method(s) from trait 'Drawable'",
    "Add method 'erase' to class 'Shape' to match trait 'Drawable'",
    "Update method 'draw' signature in 'Shape' to match trait definition"
]
```

## Usage Examples

### In Type Checking

```python
from nexuslang.typesystem.typechecker import TypeChecker

typechecker = TypeChecker()

# ... register traits and classes ...

# Check if class conforms to trait
diagnostics = typechecker.get_trait_conformance_diagnostics("MyClass", "MyTrait")

if not diagnostics['conforms']:
    print("Conformance issues found:")
    for suggestion in diagnostics['suggestions']:
        print(f"  - {suggestion}")
```

### In LSP Integration

The diagnostics can be exposed through the Language Server Protocol to provide IDE support:

```python
def publish_trait_diagnostics(class_name: str, trait_name: str) -> List[Diagnostic]:
    """Convert trait diagnostics to LSP diagnostics."""
    typechecker = TypeChecker()
    diags = typechecker.get_trait_conformance_diagnostics(class_name, trait_name)
    
    result = []
    for method in diags['missing_methods']:
        result.append(Diagnostic(
            range=find_class_definition_range(class_name),
            message=f"Missing trait method: {method}",
            severity=DiagnosticSeverity.Error
        ))
    
    for method, reason in diags['incompatible_methods'].items():
        result.append(Diagnostic(
            range=find_method_range(class_name, method),
            message=f"Incompatible signature: {reason}",
            severity=DiagnosticSeverity.Error
        ))
    
    return result
```

## Testing

Comprehensive test coverage in `tests/unit/type_system/test_trait_conformance_diagnostics.py`:

| Test Case | Purpose |
|-----------|---------|
| `test_fully_conforming_class` | Verify no errors for fully conforming classes |
| `test_missing_method` | Detect single missing method |
| `test_multiple_missing_methods` | Report all missing methods |
| `test_incompatible_method_signature` | Detect parameter count mismatches |
| `test_incompatible_return_type` | Detect return type mismatches |
| `test_trait_not_found` | Handle missing trait gracefully |
| `test_class_not_found` | Handle missing class gracefully |
| `test_diagnostic_summary_generation` | Verify helpful summary messages |
| `test_required_methods_list` | Ensure complete method list included |
| `test_metadata_in_diagnostics` | Verify diagnostic includes names |

All 10 tests passing.

## Implementation Details

### Location
- **TypeChecker Enhancement**: `src/nexuslang/typesystem/typechecker.py`
  - Added `get_trait_conformance_diagnostics()` method (~100 lines)
  - Integrates with existing `check_interface_implementation()` infrastructure

### Design Principles

1. **Non-Breaking**: Doesn't modify existing interface checking; works alongside it
2. **Comprehensive**: Returns all relevant information, not just pass/fail
3. **Actionable**: Suggestions tell developers exactly what to fix
4. **Clear**: Error messages use plain language, not jargon
5. **Extensible**: Easy to add more sophisticated checks (access modifiers, generics, etc.)

### Diagnostic Categories

1. **Missing Methods**
   - Method completely absent from implementing class
   - Trivial to fix: add the method

2. **Incompatible Signatures**
   - Method exists but doesn't match trait definition
   - Parameter count, type, or return type mismatch
   - Requires signature modification

3. **Type Resolution Issues**
   - Trait or class not found in registry
   - Forward reference or naming issue
   - May be legitimate (lazy loading, circular deps)

## Future Enhancements

### Phase 1: Current
- ✅ Basic method matching (present/missing)
- ✅ Signature compatibility checks
- ✅ Diagnostic message generation

### Phase 2: Planned
- Generic type parameter conformance
- Trait inheritance chain validation
- Access modifier conformance (public, protected, private)
- Variance checking (covariant/contravariant generics)
- Associated type resolution

### Phase 3: IDE Integration
- VS Code diagnostic provider
- Quick fix suggestions (auto-add stub methods)
- Trait implementation wizard
- Conformance monitoring in real-time

## Example Scenario

```nlpl
// Trait definition
trait Drawable
  function draw returns Unit
  function erase returns Unit
  function rotate with angle as Float returns Unit
end

// Partial implementation
class Shape
  function draw
    print text "Drawing shape"
  end
  // Missing: erase, rotate
end

// Conformance check would report:
// {
//   'conforms': false,
//   'missing_methods': ['erase', 'rotate'],
//   'incompatible_methods': {},
//   'suggestions': [
//     "Class 'Shape' must implement 2 method(s) from trait 'Drawable': erase, rotate",
//     "Add method 'erase' to class 'Shape' to match trait 'Drawable'",
//     "Add method 'rotate' to class 'Shape' to match trait 'Drawable'"
//   ],
//   'required_methods': ['draw', 'erase', 'rotate']
// }
```

## Performance

- **Computation**: O(m + n) where m = trait methods, n = class methods
- **Memory**: O(m + n) for diagnostic storage
- **Speed**: Sub-millisecond for typical traits/classes
- **Scalability**: Efficient even for large trait hierarchies

## Backward Compatibility

- ✅ No breaking changes to existing APIs
- ✅ Existing error checking still works
- ✅ New diagnostics are additive
- ✅ Optional - can be called when needed

## Testing & Validation

All tests passing:
```bash
pytest tests/unit/type_system/test_trait_conformance_diagnostics.py -v
# Result: 10 passed in 0.06s
```

Type checking remains robust while providing better developer experience through detailed, actionable diagnostics.
