# Trailing Block Syntax - Design Document

**Date:** February 11, 2026  
**Status:** Design Phase  
**Priority:** HIGH

---

## Overview

Implement Ruby/Kotlin-style trailing blocks that allow passing code blocks as the last argument to functions. This makes callback-style code and DSLs extremely readable.

---

## Design Goals

1. **Natural Language Feel** - Blocks read like English prose
2. **No Ambiguity** - Clear where block starts/ends
3. **Closure Semantics** - Capture surrounding scope
4. **Type Safe** - Blocks have function types that can be checked
5. **Flexible** - Support both with and without block parameters

---

## Proposed Syntax

### Option 1: `do...end` Blocks (Ruby-style)

```nlpl
# Simple callback
button.on_click do
    print text "Clicked!"
    update_ui
end

# With block parameters
numbers.map do |item|
    return item times 2
end

# Multiple parameters
hash_map.each do |key, value|
    print text key
    print text value
end

# Mixed regular args and block
numbers.filter with threshold: 10 do |num|
    return num is greater than threshold
end
```

**Pros:**
- Natural, reads well
- Clear block boundaries
- Familiar to Ruby developers
- `do...end` already has `do` keyword in lexer

**Cons:**
- Pipe syntax `|param|` is new for NexusLang
- Need to handle `do` in multiple contexts

### Option 2: `with...end` Extended (NLPL Native)

```nlpl
# Use existing 'with' keyword for consistency
button.on_click with
    print text "Clicked!"
    update_ui
end

# With block parameters (use 'given' or 'using')
numbers.map with item
    return item times 2
end

# Or explicitly:
numbers.map with given item
    return item times 2
end
```

**Pros:**
- Uses existing `with` keyword
- More consistent with NLPL's "natural language" style
- No new punctuation

**Cons:**
- Ambiguous: is `with` starting args or starting block?
- Harder to distinguish function args from block params

### Option 3: Hybrid Approach

```nlpl
# Regular args use 'with', block uses 'do'
numbers.filter with threshold: 10 do item
    return item is greater than threshold
end

# Standalone block
button.on_click do
    print text "Clicked!"
end

# Multiple block parameters
hash_map.each do key and value
    print text key
    print text value
end
```

**Pros:**
- Clear distinction: `with` = regular args, `do` = block
- Natural language: "do something with item"
- Consistent with NLPL's parameter style (use `and`)

**Cons:**
- Two keywords for function calls

---

## Recommended Syntax: Hybrid Approach (Option 3)

**Rationale:**
- Clearest distinction between arguments and blocks
- Uses NLPL's natural `and` separator for block params
- Maintains consistency with existing function call syntax
- `do` clearly signals "here comes code to execute"

**Syntax Rules:**

1. **Block Definition:**
   ```nlpl
   function_call do [param1 and param2 and ...]
       [body]
   end
   ```

2. **Block Parameters:** Optional, separated by `and`
   ```nlpl
   do              # No parameters
   do item         # One parameter
   do key and value  # Multiple parameters
   ```

3. **Position:** Block must be LAST argument
   ```nlpl
   function with arg1 and arg2 do
       [block body]
   end
   ```

4. **Return Values:** Use `return` to return from block
   ```nlpl
   do item
       return item times 2
   end
   ```

---

## AST Representation

### Extend FunctionCall AST

```python
class FunctionCall(Expression):
    def __init__(self, name, arguments, named_arguments=None, 
                 trailing_block=None, line_number=None):
        # ...
        self.trailing_block = trailing_block  # LambdaExpression or None
```

### Use Existing LambdaExpression

```python
# Trailing block is represented as lambda:
# do item
#     return item times 2
# end
#
# Becomes:
# LambdaExpression(
#     parameters=[Parameter("item", None)],
#     body=[ReturnStatement(BinaryOp(...))]
# )
```

---

## Parser Implementation Plan

### 1. Modify `function_call()` Method

```python
def function_call(self):
    # ... existing function call parsing ...
    
    # After parsing arguments, check for trailing block
    trailing_block = None
    if self.current_token and self.current_token.type == TokenType.DO:
        trailing_block = self.parse_trailing_block()
    
    return FunctionCall(name, args, named_args, trailing_block, line_number)
```

### 2. Add `parse_trailing_block()` Method

```python
def parse_trailing_block(self):
    """Parse a trailing block: do [params] body end"""
    line_number = self.current_token.line
    
    self.eat(TokenType.DO)
    
    # Parse optional block parameters
    params = []
    if self.current_token and self.current_token.type != TokenType.NEWLINE:
        # Parse parameters separated by 'and'
        while True:
            if self._can_be_identifier(self.current_token):
                param_name = self.current_token.lexeme
                self.advance()
                params.append(Parameter(param_name, None))  # Type inferred
                
                if self.current_token and self.current_token.type == TokenType.AND:
                    self.advance()  # Eat 'and'
                    continue
                else:
                    break
            else:
                break
    
    # Expect newline after 'do' or after parameters
    self.skip_newlines()
    
    # Parse block body (statements until 'end')
    body = []
    while self.current_token and self.current_token.type != TokenType.END:
        stmt = self.statement()
        if stmt:
            body.append(stmt)
        self.skip_newlines()
    
    self.eat(TokenType.END)
    
    # Create lambda expression for the block
    return LambdaExpression(params, body, line_number)
```

---

## Interpreter Implementation Plan

### 1. Modify `execute_FunctionCall()`

```python
def execute_FunctionCall(self, node):
    # Evaluate regular arguments
    positional_args = [self.evaluate(arg) for arg in node.arguments]
    named_args = {name: self.evaluate(expr) for name, expr in node.named_arguments.items()}
    
    # If trailing block exists, create closure and add as last argument
    if node.trailing_block:
        block_closure = self._create_closure(node.trailing_block)
        positional_args.append(block_closure)
    
    # ... rest of function call execution
```

### 2. Add `_create_closure()` Method

```python
def _create_closure(self, lambda_expr):
    """Create a closure from lambda expression, capturing current scope."""
    
    # Capture current scope
    captured_scope = self.current_scope.copy()
    
    # Create closure function
    def closure(*args):
        # Enter new scope with captured variables
        self.enter_scope()
        for var, val in captured_scope.items():
            self.set_variable(var, val)
        
        # Bind block parameters
        for i, param in enumerate(lambda_expr.parameters):
            if i < len(args):
                self.set_variable(param.name, args[i])
        
        # Execute block body
        result = None
        for stmt in lambda_expr.body:
            result = self.execute(stmt)
            if isinstance(result, ReturnValue):
                result = result.value
                break
        
        self.exit_scope()
        return result
    
    return closure
```

---

## Type Checking Plan

### 1. Infer Block Type

```python
def check_function_call(self, call, env):
    # ... existing argument checking ...
    
    if call.trailing_block:
        # Infer block type from function signature
        # If function expects FunctionType as last param, validate block matches
        expected_block_type = function_type.param_types[-1]
        
        if isinstance(expected_block_type, FunctionType):
            # Check block param count matches
            # Check block return type matches
            block_type = self.infer_lambda_type(call.trailing_block, env)
            if not block_type.is_compatible_with(expected_block_type):
                self.errors.append(f"Block type {block_type} incompatible with {expected_block_type}")
```

---

## Use Cases

### 1. Callbacks

```nlpl
button.on_click do
    print text "Button clicked!"
    update_counter
end

socket.on_data do data
    process_data with data
end
```

### 2. Collection Iteration

```nlpl
set doubled to numbers.map do item
    return item times 2
end

set filtered to numbers.filter do num
    return num is greater than 10
end

numbers.each do item
    print text item
end
```

### 3. DSLs (Domain-Specific Languages)

```nlpl
# HTML builder DSL
html.div with class: "container" do
    html.h1 do
        html.text "Welcome"
    end
    html.p do
        html.text "This is a paragraph"
    end
end

# Test framework
describe "Math operations" do
    it "should add numbers" do
        set result to add with 2 and 3
        assert result equals 5
    end
end
```

### 4. Resource Management

```nlpl
# File handling
file.open with "data.txt" do f
    set content to f.read
    process_content with content
end  # File automatically closed

# Transaction
database.transaction do
    database.insert with record1
    database.update with record2
end  # Auto commit or rollback
```

---

## Examples to Implement

### Simple Callback
```nlpl
function on_click with callback as Function
    print text "Executing callback..."
    callback
end

on_click do
    print text "Clicked!"
end
```

### Block with Parameters
```nlpl
function each_item with list as List and callback as Function
    for each item in list
        callback with item
    end
end

set numbers to [1, 2, 3]
each_item with numbers do num
    print text num times 2
end
```

### Block with Return Value
```nlpl
function map with list as List and transform as Function returns List
    set result to []
    for each item in list
        set transformed to transform with item
        result.append with transformed
    end
    return result
end

set numbers to [1, 2, 3, 4]
set doubled to map with numbers do n
    return n times 2
end
```

---

## Implementation Phases

### Phase 1: Parser Support (1 week)
- Add `parse_trailing_block()` method
- Modify `function_call()` to detect and parse blocks
- Update `FunctionCall` AST node
- Add unit tests for parsing

### Phase 2: Interpreter Support (1 week)
- Implement closure creation with scope capture
- Modify function call execution to handle trailing blocks
- Add tests for block execution

### Phase 3: Type Checking (1 week)
- Implement block type inference
- Add compatibility checking
- Test type errors

### Phase 4: Standard Library Integration (3-4 days)
- Add `map`, `filter`, `each` to collections module
- Add callback support to relevant stdlib functions
- Create comprehensive examples

### Phase 5: Documentation & Polish (2-3 days)
- Write example programs
- Update documentation
- Create tutorial
- Performance testing

**Total Estimated Time:** 3-4 weeks

---

## Compatibility Considerations

### Backward Compatibility
- Existing code without trailing blocks works unchanged
- `do` keyword only parsed after function calls
- No conflicts with existing syntax

### Future Extensions
- Multiple trailing blocks (Kotlin style)?
- Destructuring in block parameters?
- Implicit `it` parameter for single-param blocks?

---

## Open Questions

1. **Block Return Values:** Implicit return of last expression or explicit `return` only?
   - **Decision:** Support both. Last expression is implicit return unless `return` used.

2. **Scope Capture:** Capture by value or by reference?
   - **Decision:** Capture by reference (like Python closures) for mutability.

3. **Type Annotation:** Allow type hints on block parameters?
   - **Decision:** Optional. Types can be inferred from function signature.

4. **Nested Blocks:** Support blocks within blocks?
   - **Decision:** Yes, full nesting support.

5. **Early Return:** Can `return` in block exit outer function?
   - **Decision:** No. `return` exits the block only. Use exceptions for control flow.

---

## Success Criteria

- ✅ Parse `do...end` blocks after function calls
- ✅ Execute blocks with captured scope
- ✅ Support block parameters (0, 1, or multiple)
- ✅ Type check block signatures
- ✅ Integration with collections (map, filter, each)
- ✅ Comprehensive test coverage
- ✅ Example programs demonstrating all use cases
- ✅ Documentation complete

---

## Next Steps

1. **Implement parser support** - Add `parse_trailing_block()` method
2. **Update AST** - Add `trailing_block` field to `FunctionCall`
3. **Test parsing** - Create test cases for various block forms
4. **Implement interpreter** - Closure creation and execution
5. **Add type checking** - Block type inference and validation
6. **Create examples** - Demonstrate callbacks, iterators, DSLs

**Status:** Ready to implement
