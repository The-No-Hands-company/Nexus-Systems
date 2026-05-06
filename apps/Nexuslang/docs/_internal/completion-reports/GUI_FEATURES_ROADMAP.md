# NexusLang GUI Features Roadmap

**Based on:** NLPLDev field testing results 
**Date:** January 5, 2026 
**Status:** Implementation plan for GUI application support

---

## Executive Summary

Field testing in NLPLDev has revealed **NLPL is 80% ready for GUI development**. The FFI system works excellently, and console applications run perfectly. Four critical features are needed for full GUI support:

### Current Status
 **Working:** Console applications, FFI calls, basic struct parsing, extern functions 
 **Needs Implementation:** Callback functions, function pointers, complete struct support, string conversions

### Impact Assessment
- **Console apps:** 100% functional (calculator working now!)
- **ImGui:** 90% ready (no callbacks needed!)
- **SDL2 custom:** 70% ready (needs callbacks for events)
- **Native GUIs:** 50% ready (GTK/Win32 need callbacks)
- **C# Bridge:** Revolutionary approach (future game-changer)

---

## Missing Features Analysis

### 1. Callback Functions (CRITICAL)

**What it is:** Functions passed as pointers to C libraries (event handlers)

**Why we need it:**
```c
// C library expects:
void gtk_button_set_click_handler(GtkWidget* button, 
 void (*callback)(GtkWidget*, void*),
 void* user_data);

// NexusLang needs to provide:
function on_button_click that takes widget as Pointer with data as Pointer
 print text "Button clicked!"
 
# Pass to C library
call gtk_button_set_click_handler with button and 
 callback of on_button_click and null_pointer
```

**Blocks:**
- GTK3 native GUI (event handlers)
- Win32 native GUI (window procedures)
- SDL2 event callbacks
- Most modern GUI frameworks

**Workaround:** ImGui doesn't need callbacks! (Immediate mode)

**Implementation Priority:** HIGH (unlocks native GUIs)

---

### 2. Function Pointers

**What it is:** Variables that hold references to functions

**Why we need it:**
```nlpl
# Define function pointer type
type ButtonCallback as function that takes button as Pointer returns Nothing

# Store function in variable
set my_handler to reference of on_button_click

# Pass to library
call register_callback with my_handler
```

**Current NexusLang:**
```nlpl
# Can define functions
function handler that takes data as Pointer
 print text "Called"

# Can't store function reference
set callback_var to handler # Not yet supported

# Can't pass function as value
call some_lib_function with handler # Works via extern, not general case
```

**Blocks:**
- Dynamic event registration
- Plugin systems
- Strategy pattern implementations
- Higher-order functions (advanced FP)

**Implementation Priority:** HIGH (required for callbacks)

---

### 3. Complete Struct Support

**Current Status:**
```nlpl
# Parser recognizes struct syntax
struct Point
 x as Integer
 y as Integer
end

# Basic struct definition works
# Interpreter support incomplete
# Can't instantiate structs yet
# Can't access struct fields
# Can't pass structs to FFI
```

**What we need:**
```nlpl
# Define struct
struct SDL_Rect
 x as Integer
 y as Integer
 w as Integer
 h as Integer
end

# Create instance
set rect to SDL_Rect with x as 10 and y as 20 and w as 100 and h as 50

# Access fields
set width to rect.w
set rect.x to 50

# Pass to C function
call SDL_RenderFillRect with renderer and address of rect
```

**Blocks:**
- Complex data structures for GUIs
- Passing structured data to C libraries
- Reading C library returns
- Memory-efficient data layouts

**Implementation Priority:** HIGH (essential for GUI frameworks)

---

### 4. String Conversions (Pointer String)

**Current Issue:**
```nlpl
# Can declare extern with string
extern function MessageBoxA with hwnd as Integer with text as Pointer ...

# Can't convert NexusLang string to C string pointer
set message to "Hello World"
# Need: set c_string to pointer from message

# Can't read C string back to NexusLang
set result_ptr to call some_c_function # Returns char*
# Need: set nxl_string to string from result_ptr
```

**What we need:**
```nlpl
# String to C pointer
set my_text to "Hello from NLPL"
set c_ptr to c_string from my_text
call MessageBoxA with 0 and c_ptr and c_ptr and 0

# C pointer to string
set name_ptr to call GetUserName # Returns char*
set user_name to string from name_ptr
print text "User: " plus user_name
```

**Blocks:**
- Most C library string handling
- Text input from GUI frameworks
- File path manipulation
- Dynamic text generation

**Implementation Priority:** MEDIUM-HIGH (needed for practical GUIs)

---

## Implementation Priority Matrix

```

 PRIORITY MATRIX 
 
 High Impact 
 High Urgency 
 1. Callback Functions 
 - Native GUI events 
 - SDL2 interaction 
 2. Function Pointers 
 - Required for callbacks 
 - Enable dynamic dispatch 
 3. Struct Implementation 
 - Data structures 
 - FFI data passing 
 
 High Impact 
 Medium Urgency 
 4. String Conversions 
 - C string interop 
 - Text handling 
 
 Future 
 Enhancements • Advanced type system 
 • Generics for callbacks 
 • Async/await for UI 
 • C# bridge architecture 
 
```

---

## Phased Implementation Plan

### Phase 1: Foundation (Week 1-2) - Enable ImGui
**Goal:** Get first GUI application running

**Tasks:**
1. FFI system working (already done!)
2. Extern function declarations (already done!)
3. Basic struct instantiation
4. String to C pointer conversion
5. ImGui wrapper library

**Deliverable:** ImGui calculator with buttons and display

**Why ImGui first?**
- No callbacks required (immediate mode)
- Simple API
- Cross-platform
- Instant feedback

---

### Phase 2: Callbacks (Week 3-4) - Enable Native GUIs
**Goal:** Support event-driven programming

**Tasks:**
1. Function pointer type system
2. Callback registration mechanism
3. Trampoline/thunk generation for C callbacks
4. User data passing (void* context)
5. Callback lifetime management

**Deliverable:** GTK3 calculator with button callbacks

**Implementation approach:**
```python
# In interpreter.py
class CallbackWrapper:
 """Wraps NexusLang function for C callback."""
 def __init__(self, nxl_function, interpreter):
 self.nxl_function = nxl_function
 self.interpreter = interpreter
 self.c_callback = ctypes.CFUNCTYPE(...)(self._trampoline)
 
 def _trampoline(self, *args):
 """Bridge C call to NexusLang function."""
 return self.interpreter.call_function(self.nxl_function, args)
```

---

### Phase 3: Complete Structs (Week 5-6) - Data Structures
**Goal:** Full struct support with FFI integration

**Tasks:**
1. Struct instantiation syntax
2. Field access (dot notation)
3. Nested structs
4. Struct-to-ctypes conversion
5. Memory layout control (packed, aligned)
6. Sizeof operator for structs

**Deliverable:** SDL2 calculator with custom widgets

**Example:**
```nlpl
struct Button
 rect as SDL_Rect
 label as Pointer # char*
 is_pressed as Boolean
end

set button to Button with 
 rect as (SDL_Rect with x as 10 and y as 10 and w as 100 and h as 30)
 and label as c_string from "Calculate"
 and is_pressed as false
```

---

### Phase 4: String Utilities (Week 7) - Polish
**Goal:** Seamless string handling

**Tasks:**
1. `c_string from` operator (NLPL C)
2. `string from` operator (C NLPL)
3. Automatic string encoding (UTF-8/UTF-16)
4. String buffer management
5. Memory cleanup for temp strings

**Deliverable:** Professional string handling across FFI boundary

---

### Phase 5: C# Bridge (Future) - Game Changer
**Goal:** Access .NET ecosystem from NexusLang

**Concept:**
```nlpl
# Use C# interop instead of direct C FFI
use csharp namespace System.Windows.Forms

set form to new Form with Text as "Calculator"
set button to new Button with Text as "Calculate" and Location as (Point with X as 10 and Y as 10)

# Callbacks via C# delegates
set button.Click to handler of on_calculate

call form.Controls.Add with button
call form.ShowDialog
```

**Why this is revolutionary:**
- Access to WPF, WinForms, ASP.NET
- Modern UI frameworks
- Rich ecosystem
- Native Windows integration
- Cross-platform via .NET Core

---

## Quick Wins (Immediate Next Steps)

### Week 1: Basic Struct Instantiation

**Location:** `src/nlpl/interpreter/interpreter.py`

**Add to `execute_struct_definition`:**
```python
def execute_struct_definition(self, node):
 # ... existing code ...
 
 # Store struct as instantiable type
 self.struct_types[node.name] = definition
 
 # Register constructor function
 def struct_constructor(**kwargs):
 return StructureInstance(definition, kwargs)
 
 self.set_variable(node.name, struct_constructor)
```

**Add new method:**
```python
def execute_struct_instantiation(self, node):
 """Create struct instance."""
 struct_type = self.get_variable(node.struct_name)
 
 # Evaluate field values
 field_values = {}
 for field_name, value_expr in node.field_values.items():
 field_values[field_name] = self.execute(value_expr)
 
 # Create instance
 return struct_type(**field_values)
```

**Parser addition:** (likely already exists, just wire up)
```python
# In parser.py - primary() or similar
if self.match(TokenType.IDENTIFIER) and self.peek_ahead_for_struct_instantiation():
 return self.struct_instantiation()
```

**Test:**
```nlpl
struct Point
 x as Integer
 y as Integer
end

set p to Point with x as 10 and y as 20
print number p.x # Should print: 10
```

---

### Week 2: String to C Pointer

**Location:** `src/nlpl/interpreter/interpreter.py`

**Add built-in function:**
```python
def register_string_conversion_builtins(self):
 """Register string pointer conversion functions."""
 import ctypes
 
 def c_string_from_nxl(text):
 """Convert NexusLang string to C char* pointer."""
 if isinstance(text, str):
 encoded = text.encode('utf-8')
 buffer = ctypes.create_string_buffer(encoded)
 return ctypes.cast(buffer, ctypes.c_void_p).value
 return 0
 
 def string_from_c_pointer(ptr, length=None):
 """Convert C char* pointer to NexusLang string."""
 if ptr == 0 or ptr is None:
 return ""
 c_str = ctypes.c_char_p(ptr)
 return c_str.value.decode('utf-8') if c_str.value else ""
 
 self.runtime.register_function("c_string_from", c_string_from_nxl)
 self.runtime.register_function("string_from", string_from_c_pointer)
```

**Parser addition:**
```python
# In parser.py - expression parsing
if self.match(TokenType.C_STRING) or self.current_token.value == "c_string":
 self.advance() # consume 'c_string'
 self.expect(TokenType.FROM, "Expected 'from' after 'c_string'")
 string_expr = self.expression()
 return FunctionCall("c_string_from", [string_expr])
```

**Test:**
```nlpl
set message to "Hello from NLPL!"
set ptr to c_string from message

extern function puts with str as Pointer returns Integer from library "c"
call puts with ptr # Should print: Hello from NLPL!
```

---

## Feature Completion Tracker

```

 Feature Status Complexity Timeline 

 FFI System Done Complete 
 Extern Functions Done Complete 
 Struct Parsing Done Complete 
 Struct Instantiation 50% Week 1 
 Struct Field Access 30% Week 1 
 String C Pointer 0% Week 2 
 C Pointer String 0% Week 2 
 Function Pointers 0% Week 3-4 
 Callback Support 0% Week 3-4 
 Struct ctypes 40% Week 5 
 Nested Structs 0% Week 6 
 C# Interop Bridge 0% Future 

Legend:
 Done - Fully implemented and tested
 In Progress - Partial implementation
 Not Started - On roadmap
 
Complexity Scale: = Low, = High (10 blocks max)
```

---

## Success Metrics

### Phase 1 Success (ImGui)
- [ ] ImGui window opens
- [ ] Buttons respond to clicks (immediate mode)
- [ ] Text input works
- [ ] Calculator fully functional in GUI

### Phase 2 Success (Callbacks)
- [ ] GTK button callback fires
- [ ] Win32 window procedure works
- [ ] Event data passes correctly
- [ ] No crashes on callback execution

### Phase 3 Success (Structs)
- [ ] Create struct instances
- [ ] Access fields with dot notation
- [ ] Pass structs to C functions
- [ ] Receive structs from C functions
- [ ] Nested structs work

### Phase 4 Success (Strings)
- [ ] NexusLang string C string C function
- [ ] C function C string NexusLang string
- [ ] Unicode handling (UTF-8/UTF-16)
- [ ] No memory leaks

---

## Alternative Approaches

### Approach 1: ImGui First (Recommended) 
**Pros:**
- Immediate results
- No callbacks needed
- Simple API
- Cross-platform

**Cons:**
- "Tool" aesthetic, not native
- Limited to gaming/tool UIs

**Timeline:** 2 weeks to working calculator

---

### Approach 2: SDL2 Custom Widgets
**Pros:**
- Full control over appearance
- Cross-platform
- Good for custom UIs

**Cons:**
- Need to implement widgets
- Event loop needs callbacks
- More code

**Timeline:** 4 weeks to working calculator

---

### Approach 3: Native GUI First (GTK/Win32)
**Pros:**
- Native look and feel
- OS integration
- Professional results

**Cons:**
- Callbacks required first
- Platform-specific code
- Steeper learning curve

**Timeline:** 6 weeks to working calculator

---

### Approach 4: C# Bridge (Revolutionary) 
**Pros:**
- Access entire .NET ecosystem
- Modern UI frameworks (WPF, Avalonia)
- Mature libraries
- Future-proof

**Cons:**
- Complex architecture
- .NET runtime dependency
- Longer development time

**Timeline:** 12+ weeks (research + implementation)

---

## Code Examples: Before & After

### Before (Current NexusLang - Console Only)

```nlpl
# calculator_console.nlpl - Works perfectly! 

function add that takes a as Float and b as Float returns Float
 return a plus b

function multiply that takes a as Float and b as Float returns Float
 return a times b

set result to call add with 10.5 and 20.3
print text "Result: "
print number result
```

### After Phase 1 (ImGui - No Callbacks Needed)

```nlpl
# calculator_imgui.nlpl - Target implementation

use library ImGui from "cimgui"

function main
 call ImGui_CreateContext
 
 set window_open to true
 while window_open
 call ImGui_NewFrame
 
 if call ImGui_Begin with c_string from "Calculator"
 set value1 to call ImGui_InputFloat with c_string from "A"
 set value2 to call ImGui_InputFloat with c_string from "B"
 
 if call ImGui_Button with c_string from "Add"
 set result to value1 plus value2
 call ImGui_Text with c_string from ("Result: " plus result)
 
 call ImGui_End
 call ImGui_Render
 
 call ImGui_DestroyContext

call main
```

### After Phase 2 (GTK - With Callbacks)

```nlpl
# calculator_gtk.nlpl - Target implementation

use library GTK from "gtk-3"

function on_add_clicked that takes button as Pointer with data as Pointer
 set input1 to call gtk_entry_get_text with entry1
 set input2 to call gtk_entry_get_text with entry2
 
 set value1 to float from string from input1
 set value2 to float from string from input2
 set result to value1 plus value2
 
 set result_text to c_string from ("Result: " plus result)
 call gtk_label_set_text with result_label and result_text

function main
 call gtk_init with null_pointer and null_pointer
 
 set window to call gtk_window_new with 0
 set button to call gtk_button_new_with_label with c_string from "Add"
 
 # This is the key new feature - callback registration!
 call g_signal_connect with button and c_string from "clicked" 
 and callback of on_add_clicked and null_pointer
 
 call gtk_container_add with window and button
 call gtk_widget_show_all with window
 call gtk_main

call main
```

### After Phase 3 (SDL2 - With Structs)

```nlpl
# calculator_sdl.nlpl - Target implementation

struct SDL_Rect
 x as Integer
 y as Integer
 w as Integer
 h as Integer
end

struct Button
 rect as SDL_Rect
 label as Pointer
 is_pressed as Boolean
end

function draw_button that takes renderer as Pointer with button as Button
 # Access struct fields directly!
 call SDL_SetRenderDrawColor with renderer and 200 and 200 and 200 and 255
 call SDL_RenderFillRect with renderer and address of button.rect
 
 # Draw text (simplified)
 if button.is_pressed
 call SDL_SetRenderDrawColor with renderer and 100 and 100 and 100 and 255

function main
 call SDL_Init with 0x20 # SDL_INIT_VIDEO
 
 set window to call SDL_CreateWindow with 
 c_string from "Calculator" and 100 and 100 and 400 and 300 and 0
 set renderer to call SDL_CreateRenderer with window and -1 and 0
 
 # Create button struct!
 set add_button to Button with
 rect as (SDL_Rect with x as 10 and y as 10 and w as 100 and h as 30)
 and label as c_string from "Add"
 and is_pressed as false
 
 set running to true
 while running
 call draw_button with renderer and add_button
 call SDL_RenderPresent with renderer
 
 call SDL_DestroyRenderer with renderer
 call SDL_DestroyWindow with window
 call SDL_Quit

call main
```

---

## Learning Resources

### For NexusLang Contributors

**Must Read:**
1. `src/nlpl/interpreter/interpreter.py` - Execution engine
2. `src/nlpl/parser/parser.py` - Syntax parsing
3. `src/nlpl/runtime/structures.py` - Struct implementation (partial)
4. Python `ctypes` documentation - FFI bridge
5. NLPLDev test results - Real-world findings

**Callback Implementation References:**
- Python `ctypes.CFUNCTYPE` - C callback wrapper
- LuaJIT FFI - Callback trampoline technique
- PyO3 (Rust-Python) - Safe callback handling

**Struct Implementation References:**
- Python `ctypes.Structure` - Memory layout
- CFFI struct handling
- LLVM struct types

---

## Development Workflow

### Setting Up Development Environment

```bash
# 1. Clone both repositories
cd ~/dev
git clone https://github.com/Zajfan/NLPL.git
cd NexusLang

# 2. Link NLPLDev for testing
cd ~/dev
ls NLPLDev # Your test workspace

# 3. Install development dependencies
pip install -e ".[dev]"
pip install ctypes-utils # For FFI debugging

# 4. Run tests
pytest tests/
python3 src/main.py ~/dev/NLPLDev/apps/calculator_console.nlpl
```

### Testing New Features

```bash
# 1. Implement feature in NexusLang
# Edit: src/nlpl/interpreter/interpreter.py

# 2. Create test in NLPLDev
cd ~/dev/NLPLDev/experiments
nano test_new_feature.nlpl

# 3. Run test
python3 ~/dev/NexusLang/src/main.py test_new_feature.nlpl

# 4. Debug with verbose output
python3 ~/dev/NexusLang/src/main.py test_new_feature.nlpl --debug
```

---

## Expected Impact

### Developer Experience

**Before (Console Only):**
```
NLPL Program Console Output
- Text-based UI only
- Limited interaction
- No visual feedback
```

**After Phase 1 (ImGui):**
```
NLPL Program ImGui Window Interactive GUI
- Buttons, inputs, labels
- Immediate visual feedback
- Cross-platform UI
```

**After Phase 2 (Callbacks):**
```
NLPL Program Native GUI Framework Professional Apps
- GTK on Linux
- Win32 on Windows
- Event-driven architecture
- Full GUI capabilities
```

**After Phase 4 (Complete):**
```
NLPL Program Any C/C++ Library Unlimited Possibilities
- Game engines
- CAD software
- Multimedia apps
- System utilities
```

---

## Final Recommendations

### Immediate Action Plan (This Week)

1. **Monday-Tuesday:** Implement struct instantiation
 - Start with simple test case
 - Get `Point with x as 10 and y as 20` working
 - Add field access: `point.x`

2. **Wednesday-Thursday:** String conversion utilities
 - Add `c_string from` operator
 - Test with MessageBoxA (Win32)
 - Add `string from` operator

3. **Friday:** ImGui wrapper prototype
 - Create basic window
 - Add button (immediate mode, no callback)
 - Test rendering loop

### This Month Goal

**Deliverable:** Working ImGui calculator in NexusLang
- Opens window
- Has input fields
- Buttons work (immediate mode)
- Displays results
- Runs on Linux, Windows, Mac

### This Quarter Goal

**Deliverable:** Native GUI calculator with callbacks
- GTK3 on Linux
- Win32 on Windows
- Full event handling
- Professional appearance

---

## The Big Picture

NLPL is **already 80% ready** for GUI development. The FFI system is solid, console apps work perfectly, and the architecture is sound. With 4 focused weeks of development:

**Week 1-2:** ImGui calculator (immediate win) 
**Week 3-4:** Callback support (game changer) 
**Week 5-6:** Complete structs (professional apps) 
**Week 7:** Polish and documentation

After this, NexusLang becomes a **true systems programming language** capable of:
- Desktop applications
- Game development
- System utilities 
- Cross-platform tools
- Anything C/C++ can do!

**The foundation is solid. Let's build the GUI layer!** 

---

## Next Steps

1. **Review this roadmap** - Validate priorities
2. **Choose implementation order** - ImGui first? Callbacks first?
3. **Set milestones** - Weekly goals
4. **Start coding** - Struct instantiation is the quick win
5. **Test continuously** - Use NLPLDev workspace
6. **Document progress** - Update this roadmap

**Questions? Concerns? Alternative approaches?** Let's discuss!

---

**Last Updated:** January 5, 2026 
**Status:** Ready for implementation 
**Next Review:** After Phase 1 completion
