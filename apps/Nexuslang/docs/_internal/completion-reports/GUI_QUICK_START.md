# NexusLang GUI Quick Start Implementation Guide

**Goal:** Get first GUI feature working THIS WEEK 
**Date:** January 5, 2026 
**Target:** Struct instantiation + string conversion

---

## Week 1 Goals (Achievable!)

### Day 1-2: Struct Instantiation
 Parser already recognizes struct syntax 
 Wire up interpreter to create instances 
 Enable field access with dot notation

### Day 3-4: String to C Pointer
 Add `c_string from` operator 
 Test with Win32 MessageBox 
 Verify with GTK/SDL functions

### Day 5: Integration Test
 Combine structs + strings in real example 
 Document working patterns 
 Update test suite

---

## Implementation Checklist

### Part 1: Struct Instantiation (2-3 hours)

**File:** `src/nlpl/interpreter/interpreter.py`

**Step 1: Update execute_struct_definition**

Find this method (around line 558):
```python
def execute_struct_definition(self, node):
```

Add after existing code:
```python
 # Make struct instantiable
 self.struct_types[node.name] = definition
 
 # Register as a callable that creates instances
 def struct_constructor(**kwargs):
 return StructureInstance(definition, kwargs)
 
 self.set_variable(node.name, struct_constructor)
 
 return None # Struct definition returns nothing
```

**Step 2: Add struct instantiation handler**

Add new method around line 600:
```python
def execute_struct_instantiation(self, node):
 """Handle: Point with x as 10 and y as 20"""
 struct_constructor = self.get_variable(node.struct_name)
 
 if not callable(struct_constructor):
 raise RuntimeError(f"{node.struct_name} is not a struct type")
 
 # Evaluate field initializers
 field_values = {}
 for field_name, value_expr in node.fields.items():
 field_values[field_name] = self.execute(value_expr)
 
 # Create and return instance
 instance = struct_constructor(**field_values)
 return instance
```

**Step 3: Add field access handler**

Add new method:
```python
def execute_field_access(self, node):
 """Handle: my_point.x"""
 obj = self.execute(node.object)
 
 if isinstance(obj, StructureInstance):
 return obj.get_field(node.field_name)
 
 raise RuntimeError(f"Cannot access field {node.field_name} on {type(obj)}")

def execute_field_assignment(self, node):
 """Handle: set my_point.x to 50"""
 obj = self.execute(node.object)
 value = self.execute(node.value)
 
 if isinstance(obj, StructureInstance):
 obj.set_field(node.field_name, value)
 return value
 
 raise RuntimeError(f"Cannot set field {node.field_name} on {type(obj)}")
```

**Step 4: Update StructureInstance class**

File: `src/nlpl/runtime/structures.py`

Add these methods if missing:
```python
class StructureInstance:
 def __init__(self, definition, field_values):
 self.definition = definition
 self.fields = {}
 
 # Initialize all fields
 for field_name, field_type in definition.fields.items():
 if field_name in field_values:
 self.fields[field_name] = field_values[field_name]
 else:
 # Default initialization
 self.fields[field_name] = self._default_value(field_type)
 
 def get_field(self, name):
 if name not in self.fields:
 raise AttributeError(f"Struct has no field '{name}'")
 return self.fields[name]
 
 def set_field(self, name, value):
 if name not in self.definition.fields:
 raise AttributeError(f"Struct has no field '{name}'")
 self.fields[name] = value
 
 def _default_value(self, type_name):
 """Get default value for type."""
 defaults = {
 'Integer': 0,
 'Float': 0.0,
 'String': "",
 'Boolean': False,
 'Pointer': 0
 }
 return defaults.get(type_name, None)
```

**Test Case 1: Basic struct**

Create `test_struct_basic.nlpl`:
```nlpl
# Define struct
struct Point
 x as Integer
 y as Integer
end

# Create instance
set p to Point with x as 10 and y as 20

# Access fields
print text "Point x: "
print number p.x
print text "Point y: "
print number p.y

# Modify field
set p.x to 50
print text "Updated x: "
print number p.x
```

Run test:
```bash
python3 src/main.py test_struct_basic.nlpl
```

Expected output:
```
Point x: 10
Point y: 20
Updated x: 50
```

---

### Part 2: String Conversion (2-3 hours)

**File:** `src/nlpl/interpreter/interpreter.py`

**Step 1: Add string conversion functions**

In `__init__` method, add:
```python
# In Interpreter.__init__, after other initialization:
self._register_string_builtins()
```

Add new method:
```python
def _register_string_builtins(self):
 """Register string C pointer conversion."""
 import ctypes
 
 # Keep track of allocated strings to prevent GC
 self._string_buffers = []
 
 def c_string_from(text):
 """NLPL string C char* pointer."""
 if not isinstance(text, str):
 text = str(text)
 
 # Encode to bytes
 encoded = text.encode('utf-8')
 
 # Create persistent buffer
 buffer = ctypes.create_string_buffer(encoded)
 self._string_buffers.append(buffer) # Prevent GC
 
 # Return pointer value
 return ctypes.cast(buffer, ctypes.c_void_p).value
 
 def string_from(ptr, max_length=1024):
 """C char* pointer NexusLang string."""
 if ptr == 0 or ptr is None:
 return ""
 
 try:
 c_str = ctypes.cast(ptr, ctypes.c_char_p)
 if c_str.value:
 return c_str.value.decode('utf-8')
 except Exception as e:
 # Handle invalid pointer gracefully
 return f"<invalid string pointer: {hex(ptr)}>"
 
 return ""
 
 # Register as built-in functions
 self.runtime.register_function("c_string_from", c_string_from)
 self.runtime.register_function("string_from", string_from)
```

**Step 2: Add syntax support (if needed)**

File: `src/nlpl/parser/parser.py`

Check if `c_string from` is already parseable. If not, add:
```python
# In expression parsing (primary or similar)
if self.current_token.value == 'c_string':
 self.advance()
 self.expect_keyword('from')
 string_expr = self.expression()
 return FunctionCall('c_string_from', [string_expr])

if self.current_token.value == 'string':
 self.advance()
 self.expect_keyword('from')
 ptr_expr = self.expression()
 return FunctionCall('string_from', [ptr_expr])
```

**Test Case 2: String to C pointer (Windows)**

Create `test_string_windows.nlpl`:
```nlpl
# Test string conversion with Windows MessageBox

# Declare MessageBox function
extern function MessageBoxA with hwnd as Integer with text as Pointer 
 with caption as Pointer with type as Integer returns Integer 
 from library "user32" calling convention stdcall

# Convert strings to C pointers
set message to "Hello from NLPL!"
set title to "NLPL Test"

set msg_ptr to c_string from message
set title_ptr to c_string from title

# Call Windows API
set result to call MessageBoxA with 0 and msg_ptr and title_ptr and 0

print text "MessageBox returned: "
print number result
```

**Test Case 3: String conversion (Linux)**

Create `test_string_linux.nlpl`:
```nlpl
# Test with C standard library

extern function puts with str as Pointer returns Integer from library "c"
extern function strlen with str as Pointer returns Integer from library "c"

set message to "Hello from NexusLang on Linux!"
set c_str to c_string from message

# Get string length
set length to call strlen with c_str
print text "String length: "
print number length

# Print string
call puts with c_str
```

---

### Part 3: Integration Test (1-2 hours)

**Test Case 4: Struct + String together**

Create `test_integrated.nlpl`:
```nlpl
# Combine structs and string handling

struct Person
 name as Pointer
 age as Integer
end

# Create person
set name_str to c_string from "Alice"
set person to Person with name as name_str and age as 30

# Access and print
print text "Name pointer: "
print number person.name

print text "Age: "
print number person.age

# Convert C string back
set readable_name to string from person.name
print text "Name: "
print text readable_name
```

---

## Testing Strategy

### Quick Test Script

Create `test_gui_features.sh`:
```bash
#!/bin/bash
# Test new GUI features

echo "=== Testing Struct Instantiation ==="
python3 src/main.py test_struct_basic.nlpl

echo ""
echo "=== Testing String Conversion (Linux) ==="
python3 src/main.py test_string_linux.nlpl

echo ""
echo "=== Testing Integration ==="
python3 src/main.py test_integrated.nlpl

echo ""
echo " All tests complete!"
```

Make executable:
```bash
chmod +x test_gui_features.sh
```

---

## Common Issues & Solutions

### Issue 1: "StructureInstance not found"

**Solution:** Import in interpreter.py:
```python
from nexuslang.runtime.structures import StructureInstance
```

### Issue 2: "Field access not working"

**Check parser:** Does AST have FieldAccess node?
```python
# In parser/ast.py
class FieldAccess:
 def __init__(self, object, field_name):
 self.object = object
 self.field_name = field_name
```

### Issue 3: "String pointer causes crash"

**Solution:** Ensure buffer persists:
```python
# Store buffer reference
self._string_buffers.append(buffer)
```

### Issue 4: "Struct fields all None"

**Check:** Field initialization in StructureInstance:
```python
def __init__(self, definition, field_values):
 # Must initialize fields dict!
 self.fields = {}
 for name, value in field_values.items():
 self.fields[name] = value
```

---

## Progress Tracking

### Completion Checklist

**Day 1:**
- [ ] Read existing struct implementation
- [ ] Add struct_types dictionary to interpreter
- [ ] Implement struct_constructor function
- [ ] Test basic struct creation

**Day 2:**
- [ ] Implement field access (get)
- [ ] Implement field assignment (set)
- [ ] Run test_struct_basic.nlpl
- [ ] Fix any issues

**Day 3:**
- [ ] Implement c_string_from
- [ ] Test on Windows (MessageBox)
- [ ] Test on Linux (puts/strlen)

**Day 4:**
- [ ] Implement string_from
- [ ] Add buffer management
- [ ] Test round-trip conversion

**Day 5:**
- [ ] Create integrated test
- [ ] Document working examples
- [ ] Update main test suite
- [ ] Commit and push

---

## Success Criteria

By end of week, these should work:

```nlpl
# 1. Create struct
struct Point
 x as Integer
 y as Integer
end

set p to Point with x as 10 and y as 20

# 2. Access fields
print number p.x # Prints: 10

# 3. Modify fields
set p.x to 50
print number p.x # Prints: 50

# 4. Convert strings
set c_str to c_string from "Hello"
extern function puts with s as Pointer returns Integer from library "c"
call puts with c_str # Prints: Hello

# 5. Integration
struct Person
 name as Pointer
 age as Integer
end

set person to Person with 
 name as (c_string from "Bob") 
 and age as 25
 
set name_back to string from person.name
print text name_back # Prints: Bob
```

---

## After This Week

With structs and strings working, you can:

### Week 2: ImGui Prototype

```nlpl
use library ImGui from "cimgui"

# Now we can!
set window_title to c_string from "My First GUI"
call ImGui_Begin with window_title

# And structs for data!
struct CalculatorState
 value1 as Float
 value2 as Float
 result as Float
end

set state to CalculatorState with 
 value1 as 0.0 and value2 as 0.0 and result as 0.0
```

### Week 3-4: Full ImGui Calculator

```nlpl
# Complete working GUI calculator!
function main
 call ImGui_CreateContext
 
 set state to CalculatorState with all fields as 0.0
 
 while true
 call ImGui_NewFrame
 
 if call ImGui_Begin with c_string from "Calculator"
 # Input fields
 call ImGui_InputFloat with c_string from "A" 
 and address of state.value1
 call ImGui_InputFloat with c_string from "B" 
 and address of state.value2
 
 # Buttons
 if call ImGui_Button with c_string from "Add"
 set state.result to state.value1 plus state.value2
 
 # Display
 set result_text to "Result: " plus state.result
 call ImGui_Text with c_string from result_text
 
 call ImGui_End
 call ImGui_Render
 
 call ImGui_DestroyContext
```

---

## References

**Key Files:**
- `src/nlpl/interpreter/interpreter.py` - Execution engine
- `src/nlpl/runtime/structures.py` - Struct definitions
- `src/nlpl/parser/ast.py` - AST node types
- `NLPLDev/apps/calculator_gui_*.nlpl` - Target examples

**Python Docs:**
- `ctypes.Structure` - Memory layout reference
- `ctypes.create_string_buffer` - String buffers
- `ctypes.CFUNCTYPE` - Callbacks (for later)

**Testing:**
- `NLPLDev/apps/calculator_console.nlpl` - Working baseline
- `examples/23_pointer_operations.nlpl` - Pointer examples
- `examples/24_struct_and_union.nlpl` - Struct examples

---

## Quick Wins First Approach

Focus on **minimum viable implementation**:

1. Don't worry about nested structs yet
2. Don't worry about struct methods yet
3. Don't worry about advanced memory management
4. Get basic instantiation + field access working
5. Get string conversion working
6. Test with real GUI code

**Then iterate** - Add complexity after basics work!

---

## Learning by Doing

### Best Way to Learn Implementation

1. **Read existing code:**
 ```bash
 grep -r "StructDefinition" src/nlpl/interpreter/
 ```

2. **Start small:**
 - First: Just create struct instance
 - Then: Access one field
 - Then: Modify field
 - Finally: Complex cases

3. **Test immediately:**
 - Write test after each feature
 - Run test before moving on
 - Fix issues as you find them

4. **Use debugger:**
 ```bash
 python3 -m pdb src/main.py test_struct_basic.nlpl
 ```

---

## Deliverable

By Friday (end of week):

**File:** `test_programs/integration/gui_features.nlpl`

```nlpl
# GUI Features Integration Test
# Tests: Struct instantiation + String conversion

print text "=== Struct Test ==="

struct Point
 x as Integer
 y as Integer
end

set p to Point with x as 10 and y as 20
print number p.x
print number p.y

set p.x to 50
print number p.x

print text "=== String Conversion Test ==="

extern function puts with s as Pointer returns Integer from library "c"

set message to "Hello from NLPL!"
set c_str to c_string from message
call puts with c_str

print text "=== Integration Test ==="

struct Label
 text as Pointer
 x as Integer
 y as Integer
end

set label to Label with 
 text as (c_string from "Button") 
 and x as 100 
 and y as 50

set text_back to string from label.text
print text "Label: "
print text text_back
print text " at ("
print number label.x
print text ","
print number label.y
print text ")"

print text "=== All Tests Passed! ==="
```

**Expected Output:**
```
=== Struct Test ===
10
20
50
=== String Conversion Test ===
Hello from NLPL!
=== Integration Test ===
Label: Button at (100,50)
=== All Tests Passed! ===
```

---

**Let's make NexusLang GUI-capable THIS WEEK!** 

**Questions? Start with:** `src/nlpl/interpreter/interpreter.py` line 558 
**Stuck?** Check NLPLDev examples for working patterns 
**Ready?** Let's code! 
