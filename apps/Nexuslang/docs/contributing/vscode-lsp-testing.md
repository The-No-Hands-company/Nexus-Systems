# VS Code LSP Integration Testing Guide

**Date:** February 16, 2026  
**Phase:** Week 1 - Day 2/3  
**Goal:** Validate all 13 LSP features in VS Code with NexusLang workspace

---

## Overview

This guide describes how to:
1. Install and configure the NexusLang LSP extension in VS Code
2. Test all 13 implemented LSP features
3. Validate performance and user experience
4. Debug issues and collect metrics

---

## Prerequisites

**Required:**
- VS Code 1.75.0 or later
- Node.js 16+ and npm (for extension build)
- Python 3.10+ (for LSP server)
- NexusLang workspace (examples/ or test_programs/)

**Optional:**
- VS Code Extension Test Runner
- Chrome DevTools (for extension debugging)

---

## Installation Steps

### Method 1: Local Extension Development (Recommended)

1. **Navigate to extension directory:**
   ```bash
   cd vscode-extension/
   ```

2. **Install dependencies (if not already done):**
   ```bash
   npm install
   ```

3. **Compile TypeScript:**
   ```bash
   npm run compile
   ```

4. **Open extension in VS Code:**
   ```bash
   code vscode-extension/
   ```

5. **Press F5** to launch Extension Development Host
   - A new VS Code window opens with the extension loaded
   - Opens NexusLang workspace automatically

### Method 2: Install Extension Globally

1. **Package extension:**
   ```bash
   cd vscode-extension/
   npm run package
   ```

2. **Install .vsix file:**
   ```bash
   code --install-extension nlpl-language-support-0.1.0.vsix
   ```

3. **Reload VS Code:**
   ```
   Ctrl+Shift+P → "Developer: Reload Window"
   ```

---

## Configuration

### Extension Settings

Open VS Code settings (Ctrl+,) and search for "NexusLang":

```json
{
  "nexuslang.languageServer.enabled": true,
  "nexuslang.languageServer.pythonPath": "python3",
  "nexuslang.languageServer.path": "",  // Leave empty for auto-detect
  "nexuslang.trace.server": "verbose",  // For debugging
  "nexuslang.languageServer.debug": false,
  "nexuslang.languageServer.logFile": ""
}
```

**Note:** If `path` is empty, extension automatically finds `src/nexuslang/lsp/server.py` in workspace.

### Verify LSP Server is Running

1. **Open Output panel:** View → Output (Ctrl+Shift+U)
2. **Select channel:** "NLPL Language Server" from dropdown
3. **Check for startup message:**
   ```
   Starting NexusLang Language Server...
   Workspace root: /path/to/NLPL
   Indexing workspace...
   Found 41 files
   Indexed 718 symbols
   Server ready
   ```

---

## Testing Checklist

### Test Workspace Setup

**Use these test files:**
- `examples/01_basic_concepts.nlpl` - Variables, functions
- `examples/08_classes_and_methods.nlpl` - Classes, methods
- `examples/12_module_system.nlpl` - Imports
- `test_programs/unit/stdlib/test_math.nlpl` - stdlib calls

---

## Feature 1: Go to Definition (F12)

**Purpose:** Jump to symbol definition (cross-file support)

### Test Cases

1. **Local variable definition:**
   - Open `examples/01_basic_concepts.nlpl`
   - Place cursor on `message` usage (line ~10)
   - Press **F12** or Ctrl+Click
   - **Expected:** Jump to `set message to "..."` declaration

2. **Function definition:**
   - Place cursor on function call: `greet(name)`
   - Press **F12**
   - **Expected:** Jump to `function greet with name as String` definition

3. **Cross-file definition:**
   - Open file that imports another module
   - Place cursor on imported function call
   - Press **F12**
   - **Expected:** Open defining file and jump to function

4. **Class method definition:**
   - Open `examples/08_classes_and_methods.nlpl`
   - Place cursor on method call: `person.get_full_name()`
   - Press **F12**
   - **Expected:** Jump to method definition inside class

**Performance:** Definition should appear <100ms

---

## Feature 2: Find All References (Shift+F12)

**Purpose:** Find all usages of a symbol across workspace

### Test Cases

1. **Find variable references:**
   - Place cursor on variable `name`
   - Press **Shift+F12**
   - **Expected:** Peek window shows all references (declaration + usages)

2. **Find function references:**
   - Place cursor on function `greet`
   - Press **Shift+F12**
   - **Expected:** Shows all call sites across all files

3. **Find class references:**
   - Place cursor on class name `Person`
   - Press **Shift+F12**
   - **Expected:** Shows class definition + all instantiations

**Performance:** Results should appear <200ms for 41 files

---

## Feature 3: Rename Symbol (F2)

**Purpose:** Rename symbol and update all references

### Test Cases

1. **Rename local variable:**
   - Place cursor on `message`
   - Press **F2**
   - Type new name: `greeting_text`
   - Press Enter
   - **Expected:** All references updated, no syntax errors

2. **Rename function:**
   - Place cursor on function `greet`
   - Press **F2**
   - Type `say_hello`
   - **Expected:** Function definition + all call sites updated

3. **Rename class:**
   - Place cursor on `Person`
   - Press **F2**
   - Type `Employee`
   - **Expected:** Class definition + all instantiations updated

**Performance:** Rename should complete <500ms

---

## Feature 4: Document Outline (Ctrl+Shift+O)

**Purpose:** Quick navigation within file (hierarchical symbol view)

### Test Cases

1. **Navigate to function:**
   - Open any .nlpl file
   - Press **Ctrl+Shift+O**
   - **Expected:** Dropdown shows all functions, classes, methods
   - Type function name to filter
   - Press Enter to jump

2. **Hierarchical view:**
   - Open file with classes and methods
   - Press **Ctrl+Shift+O**
   - **Expected:** Classes shown, methods indented underneath

3. **Symbol icons:**
   - **Expected:** Functions have `ƒ` icon, Classes have `C`, Methods have `m`

**Performance:** Outline should populate <50ms

---

## Feature 5: Workspace Symbols (Ctrl+T)

**Purpose:** Search for symbols across entire workspace

### Test Cases

1. **Fuzzy search:**
   - Press **Ctrl+T**
   - Type `greet`
   - **Expected:** Shows all symbols containing "greet" (case-insensitive)

2. **Partial match:**
   - Type `calc`
   - **Expected:** Shows `calculate_average`, `Calculator` class, etc.

3. **Filter by symbol type:**
   - Type `#greet` (# = function filter)
   - **Expected:** Shows only functions

**Performance:** Search results <100ms for 718 symbols

---

## Feature 6: Call Hierarchy (Shift+Alt+H)

**Purpose:** Show who calls this function (incoming) and what it calls (outgoing)

### Test Cases

1. **Incoming calls:**
   - Place cursor on function `greet`
   - Press **Shift+Alt+H**
   - Select "Show Incoming Calls"
   - **Expected:** Tree view shows all functions that call `greet`

2. **Outgoing calls:**
   - Place cursor on `main` function
   - Press **Shift+Alt+H**
   - Select "Show Outgoing Calls"
   - **Expected:** Tree view shows all functions called by `main`

3. **Nested call hierarchy:**
   - Expand incoming caller
   - **Expected:** Shows who calls the caller (recursive expansion)

**Performance:** Hierarchy should build <200ms

---

## Feature 7: Hover Documentation (Mouse Hover)

**Purpose:** Show symbol information on hover

### Test Cases

1. **Function hover:**
   - Hover over function name
   - **Expected:** Popup shows:
     - Function signature
     - Parameter types
     - Return type
     - Docstring (if available)

2. **Variable hover:**
   - Hover over variable
   - **Expected:** Shows type and value (if literal)

3. **Class hover:**
   - Hover over class name
   - **Expected:** Shows class definition and members

**Performance:** Hover popup <100ms

---

## Feature 8: Code Completion (Ctrl+Space)

**Purpose:** Auto-complete suggestions for symbols

### Test Cases

1. **Function name completion:**
   - Type `gre` + **Ctrl+Space**
   - **Expected:** Suggestions include `greet` function

2. **Method completion after dot:**
   - Type `person.` + **Ctrl+Space**
   - **Expected:** Shows all methods of `Person` class

3. **Import suggestions:**
   - Type `import ` + **Ctrl+Space**
   - **Expected:** Shows available modules

**Performance:** Suggestions <100ms

---

## Feature 9: Diagnostics (Real-time Errors)

**Purpose:** Show syntax/semantic errors as you type

### Test Cases

1. **Syntax error:**
   - Type invalid syntax: `set x to`
   - **Expected:** Red underline + error message in Problems panel

2. **Undefined variable:**
   - Use undeclared variable: `print text undefined_var`
   - **Expected:** Error: "Undefined variable: undefined_var"

3. **Type mismatch:**
   - Pass wrong type to function
   - **Expected:** Type error diagnostic

**Performance:** Diagnostics appear <500ms after typing stops

---

## Feature 10: Code Actions (Ctrl+.)

**Purpose:** Quick fixes for common problems

### Test Cases

1. **Extract method:**
   - Select code block
   - Press **Ctrl+.**
   - **Expected:** "Extract to function" option

2. **Import missing symbol:**
   - Use undefined symbol
   - Press **Ctrl+.**
   - **Expected:** "Import from module" suggestion

**Performance:** Code actions <100ms

---

## Feature 11: Signature Help (Ctrl+Shift+Space)

**Purpose:** Show function parameters while typing call

### Test Cases

1. **Parameter hints:**
   - Type `greet(`
   - **Expected:** Popup shows: `greet(name: String) -> None`
   - Highlights current parameter

2. **Multiple parameters:**
   - Type function call with multiple params
   - **Expected:** Bold highlights current parameter as you type

**Performance:** Signature popup <50ms

---

## Feature 12: Formatting (Shift+Alt+F)

**Purpose:** Auto-format code

### Test Cases

1. **Indent correction:**
   - Create badly indented code
   - Press **Shift+Alt+F**
   - **Expected:** Code properly indented

2. **Whitespace normalization:**
   - Add extra spaces around operators
   - Format
   - **Expected:** Consistent spacing

**Performance:** Format entire file <200ms

---

## Feature 13: Semantic Tokens (Syntax Highlighting)

**Purpose:** Accurate syntax coloring based on semantic analysis

### Test Cases

1. **Keyword highlighting:**
   - **Expected:** `function`, `set`, `to`, `if`, `else` colored as keywords

2. **Function vs variable colors:**
   - **Expected:** Function names and variable names have different colors

3. **Type annotations:**
   - **Expected:** `String`, `Integer` colored as types

**Performance:** Tokens update <100ms after edit

---

## Performance Benchmarks

### Expected Performance Metrics

| Feature | Target Latency | Acceptable | Unacceptable |
|---------|---------------|-----------|--------------|
| Go to definition | <50ms | <100ms | >200ms |
| Find references | <100ms | <200ms | >500ms |
| Rename | <200ms | <500ms | >1s |
| Document outline | <50ms | <100ms | >200ms |
| Workspace symbols | <100ms | <200ms | >500ms |
| Call hierarchy | <100ms | <200ms | >500ms |
| Hover | <50ms | <100ms | >200ms |
| Completion | <50ms | <100ms | >200ms |
| Diagnostics | <200ms | <500ms | >1s |
| Code actions | <50ms | <100ms | >200ms |
| Signature help | <50ms | <100ms | >200ms |
| Formatting | <100ms | <200ms | >500ms |
| Semantic tokens | <50ms | <100ms | >200ms |

### How to Measure

**Method 1: VS Code Performance Profiler**
1. Open Command Palette (Ctrl+Shift+P)
2. Run "Developer: Show Running Extensions"
3. Look for "NLPL Language Support"
4. Check activation time and memory usage

**Method 2: LSP Trace Logs**
1. Set `"nexuslang.trace.server": "verbose"`
2. Open Output panel → "NLPL LSP Trace"
3. Observe request/response times:
   ```
   [Trace - 14:23:45] Sending request 'textDocument/definition'
   [Trace - 14:23:45] Received response 'textDocument/definition' - 45ms
   ```

**Method 3: Chrome DevTools**
1. Help → Toggle Developer Tools
2. Profile tab → Record CPU Profile
3. Perform LSP action
4. Stop recording and analyze

---

## Debugging Issues

### LSP Server Not Starting

**Symptoms:** No autocomplete, no go-to-definition, "NLPL Language Server" output empty

**Debug steps:**
1. Check Python path:
   ```bash
   python3 --version
   ```

2. Test LSP server manually:
   ```bash
   python3 -m nexuslang.lsp --stdio
   ```
   Type `{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}` + Enter
   Expected: JSON response with server capabilities

3. Check extension logs:
   - View → Output → "NLPL Language Server"
   - Look for error messages

4. Verify file is recognized as NexusLang:
   - Bottom right of VS Code → should say "NexusLang"
   - If not, click and select "NexusLang" manually

### Slow Performance

**Symptoms:** Features take >1s to respond

**Debug steps:**
1. Check workspace size:
   ```bash
   find . -name "*.nxl" | wc -l
   ```
   If >100 files, indexing may be slow

2. Enable trace logging:
   ```json
   "nexuslang.trace.server": "verbose"
   ```
   Check for repeated re-indexing

3. Profile LSP server:
   ```bash
   python3 dev_tools/profile_lsp.py .
   ```

### Features Not Working

**Symptoms:** Specific feature (e.g., go-to-definition) doesn't work

**Debug steps:**
1. Check LSP capabilities response:
   - Look in "NLPL LSP Trace" output
   - Find `initialize` response
   - Verify capability is advertised:
     ```json
     {
       "capabilities": {
         "definitionProvider": true,
         "referencesProvider": true,
         ...
       }
     }
     ```

2. Check for Python exceptions:
   - "NLPL Language Server" output
   - Look for traceback

3. Test with minimal example:
   - Create simple test.nlpl with basic code
   - Try feature on that file

---

## Test Results Template

**Test Date:** YYYY-MM-DD  
**VS Code Version:** X.Y.Z  
**Extension Version:** 0.1.0  
**Workspace:** examples/ (41 files, 718 symbols)

### Feature Test Results

| Feature | Status | Latency | Notes |
|---------|--------|---------|-------|
| Go to Definition | ✅ | 45ms | Cross-file works |
| Find References | ✅ | 120ms | All refs found |
| Rename | ✅ | 230ms | Updates all files |
| Document Outline | ✅ | 35ms | Hierarchy correct |
| Workspace Symbols | ✅ | 85ms | Fuzzy search works |
| Call Hierarchy | ✅ | 145ms | Incoming/outgoing OK |
| Hover | ✅ | 40ms | Shows signatures |
| Completion | ✅ | 55ms | Relevant suggestions |
| Diagnostics | ✅ | 280ms | Real-time errors |
| Code Actions | ⚠️ | 75ms | Limited actions |
| Signature Help | ✅ | 30ms | Param highlighting |
| Formatting | ✅ | 110ms | Proper indentation |
| Semantic Tokens | ✅ | 45ms | Accurate colors |

**Overall:** ✅ 12/13 passing, ⚠️ 1 partial, ❌ 0 failing

**Performance:** All features <200ms (target met)

**Issues:**
- Code actions: Only extract method available, need more quick fixes

**Recommendations:**
- Add more code action providers
- Consider caching for large workspaces

---

## Next Steps

1. **Complete full test pass** with checklist
2. **Document issues** in GitHub/project tracker
3. **Measure performance** with benchmarks
4. **Create demo video** showing all features
5. **Optimize based on profiling** (AST caching, parallel indexing)

---

## Appendix: Extension Development

### Adding New LSP Features

1. **Implement in server.py:**
   ```python
   def _handle_new_feature(self, msg_id: int, params: Dict) -> Dict:
       # Implementation
       return {"result": ...}
   ```

2. **Advertise capability:**
   ```python
   "capabilities": {
       "newFeatureProvider": True
   }
   ```

3. **Test with VS Code** (no extension changes needed - LSP client auto-detects)

### Updating Extension

1. **Modify `src/extension.ts`** (if needed)
2. **Recompile:**
   ```bash
   npm run compile
   ```
3. **Reload Extension Development Host:** Ctrl+R in extension window

---

## Resources

- **VS Code LSP Guide:** https://code.visualstudio.com/api/language-extensions/language-server-extension-guide
- **LSP Specification:** https://microsoft.github.io/language-server-protocol/
- **NexusLang LSP Features:** `docs/tooling/lsp-features.md`
- **LSP Guide:** `docs/tooling/lsp.md`
