# VS Code LSP Testing Checklist
**Date:** February 16, 2026  
**Extension:** vscode-extension/  
**Status:** Ready for testing

---

## Pre-Flight Verification ✅

- [x] All 24 LSP tests passing (100%)
- [x] Extension compiled (out/extension.js)
- [x] Package exists (nlpl-language-support-0.1.0.vsix)
- [x] LSP server ready (src/nlpl/lsp/server.py)
- [x] Performance profiling complete

---

## Testing Session Instructions

### Step 1: Launch Extension Development Host

1. **Open extension in VS Code:**
   ```bash
   code vscode-extension/
   ```

2. **Press F5** (or Run → Start Debugging)
   - This launches a new VS Code window titled **"[Extension Development Host]"**
   - Extension is automatically loaded and active

3. **Verify LSP server starts:**
   - In Extension Development Host, open **Output panel** (Ctrl+Shift+U)
   - Select **"NLPL Language Server"** from dropdown
   - Look for startup messages:
     ```
     Starting NexusLang Language Server...
     Workspace root: /path/to/NLPL
     Indexing workspace...
     Found 41 files
     Indexed 718 symbols
     Server ready
     ```

4. **Open test file:**
   - File → Open Folder → Select NexusLang workspace
   - Open `examples/01_basic_concepts.nlpl`
   - Verify file is recognized as NexusLang (bottom right shows "NexusLang")

---

## Feature Testing (13 Features)

### 1. Go to Definition (F12) ⏱️ Target: <100ms

**Test 1.1: Local variable**
- [ ] Open `examples/01_basic_concepts.nlpl`
- [ ] Place cursor on variable usage (e.g., `message` on line ~10)
- [ ] Press **F12** (or Ctrl+Click)
- [ ] **Expected:** Jump to `set message to "..."` declaration
- [ ] **Latency:** _____ ms

**Test 1.2: Function call**
- [ ] Place cursor on `greet(name)` call
- [ ] Press **F12**
- [ ] **Expected:** Jump to `function greet with name as String`
- [ ] **Latency:** _____ ms

**Test 1.3: Cross-file (if module imports exist)**
- [ ] Find function call to imported symbol
- [ ] Press **F12**
- [ ] **Expected:** Open defining file and jump to definition
- [ ] **Latency:** _____ ms

**Result:** ✅ / ⚠️ / ❌

---

### 2. Find All References (Shift+F12) ⏱️ Target: <200ms

**Test 2.1: Variable references**
- [ ] Place cursor on variable `name`
- [ ] Press **Shift+F12**
- [ ] **Expected:** Peek window shows all references
- [ ] **Latency:** _____ ms

**Test 2.2: Function references**
- [ ] Place cursor on function `greet`
- [ ] Press **Shift+F12**
- [ ] **Expected:** Shows all call sites across files
- [ ] **Latency:** _____ ms

**Result:** ✅ / ⚠️ / ❌

---

### 3. Rename Symbol (F2) ⏱️ Target: <500ms

**Test 3.1: Rename variable**
- [ ] Place cursor on `message`
- [ ] Press **F2**
- [ ] Type new name: `greeting_text`
- [ ] Press Enter
- [ ] **Expected:** All references updated, no errors
- [ ] **Latency:** _____ ms

**Test 3.2: Undo rename**
- [ ] Press **Ctrl+Z** to undo
- [ ] **Expected:** Original name restored

**Result:** ✅ / ⚠️ / ❌

---

### 4. Document Outline (Ctrl+Shift+O) ⏱️ Target: <100ms

**Test 4.1: Symbol navigation**
- [ ] Open `examples/08_classes_and_methods.nlpl`
- [ ] Press **Ctrl+Shift+O**
- [ ] **Expected:** Dropdown shows functions, classes, methods
- [ ] **Latency:** _____ ms

**Test 4.2: Hierarchical view**
- [ ] Verify classes shown with methods indented underneath
- [ ] Click a method to jump to it
- [ ] **Expected:** Cursor moves to method definition

**Result:** ✅ / ⚠️ / ❌

---

### 5. Workspace Symbols (Ctrl+T) ⏱️ Target: <100ms

**Test 5.1: Fuzzy search**
- [ ] Press **Ctrl+T**
- [ ] Type `greet`
- [ ] **Expected:** Shows symbols containing "greet"
- [ ] **Latency:** _____ ms

**Test 5.2: Partial match**
- [ ] Type `calc`
- [ ] **Expected:** Shows `calculate_*` symbols
- [ ] **Latency:** _____ ms

**Result:** ✅ / ⚠️ / ❌

---

### 6. Call Hierarchy (Shift+Alt+H) ⏱️ Target: <200ms

**Test 6.1: Incoming calls**
- [ ] Place cursor on function `greet`
- [ ] Press **Shift+Alt+H**
- [ ] Select "Show Incoming Calls"
- [ ] **Expected:** Tree shows functions that call `greet`
- [ ] **Latency:** _____ ms

**Test 6.2: Outgoing calls**
- [ ] Place cursor on `main` function
- [ ] Press **Shift+Alt+H**
- [ ] Select "Show Outgoing Calls"
- [ ] **Expected:** Tree shows functions called by `main`
- [ ] **Latency:** _____ ms

**Result:** ✅ / ⚠️ / ❌

---

### 7. Hover Documentation ⏱️ Target: <100ms

**Test 7.1: Function hover**
- [ ] Hover mouse over function name
- [ ] **Expected:** Popup shows function signature
- [ ] **Latency:** _____ ms

**Test 7.2: Variable hover**
- [ ] Hover over variable
- [ ] **Expected:** Shows type information
- [ ] **Latency:** _____ ms

**Result:** ✅ / ⚠️ / ❌

---

### 8. Code Completion (Ctrl+Space) ⏱️ Target: <100ms

**Test 8.1: Function name completion**
- [ ] Type `gre` + **Ctrl+Space**
- [ ] **Expected:** Suggestions include `greet`
- [ ] **Latency:** _____ ms

**Test 8.2: Method completion after dot**
- [ ] Type `person.` + **Ctrl+Space** (if classes exist)
- [ ] **Expected:** Shows class methods
- [ ] **Latency:** _____ ms

**Result:** ✅ / ⚠️ / ❌

---

### 9. Diagnostics (Real-time Errors) ⏱️ Target: <500ms

**Test 9.1: Syntax error**
- [ ] Type invalid syntax: `set x to` (incomplete)
- [ ] Wait 1 second
- [ ] **Expected:** Red underline + error in Problems panel
- [ ] **Latency:** _____ ms

**Test 9.2: Fix error**
- [ ] Complete the line: `set x to 10`
- [ ] **Expected:** Error disappears
- [ ] **Latency:** _____ ms

**Result:** ✅ / ⚠️ / ❌

---

### 10. Code Actions (Ctrl+.) ⏱️ Target: <100ms

**Test 10.1: Available actions**
- [ ] Select a code block
- [ ] Press **Ctrl+.**
- [ ] **Expected:** Shows quick fix options (if any)
- [ ] **Latency:** _____ ms

**Note:** May show limited actions if not fully implemented

**Result:** ✅ / ⚠️ / ❌

---

### 11. Signature Help (Ctrl+Shift+Space) ⏱️ Target: <50ms

**Test 11.1: Parameter hints**
- [ ] Type `greet(`
- [ ] **Expected:** Popup shows: `greet(name: String)`
- [ ] **Latency:** _____ ms

**Test 11.2: Parameter highlighting**
- [ ] Continue typing function arguments
- [ ] **Expected:** Current parameter highlighted
- [ ] **Latency:** _____ ms

**Result:** ✅ / ⚠️ / ❌

---

### 12. Formatting (Shift+Alt+F) ⏱️ Target: <200ms

**Test 12.1: Format document**
- [ ] Create badly indented code
- [ ] Press **Shift+Alt+F**
- [ ] **Expected:** Code properly formatted
- [ ] **Latency:** _____ ms

**Result:** ✅ / ⚠️ / ❌

---

### 13. Semantic Tokens (Syntax Highlighting) ⏱️ Target: <100ms

**Test 13.1: Keyword coloring**
- [ ] Verify `function`, `set`, `to`, `if`, `else` colored as keywords
- [ ] **Expected:** Distinct colors for keywords

**Test 13.2: Symbol coloring**
- [ ] Verify function names, variables have different colors
- [ ] **Expected:** Semantic coloring active

**Result:** ✅ / ⚠️ / ❌

---

## Performance Summary

| Feature | Target | Actual | Status |
|---------|--------|--------|--------|
| Go to Definition | <100ms | _____ ms | ☐ |
| Find References | <200ms | _____ ms | ☐ |
| Rename | <500ms | _____ ms | ☐ |
| Document Outline | <100ms | _____ ms | ☐ |
| Workspace Symbols | <100ms | _____ ms | ☐ |
| Call Hierarchy | <200ms | _____ ms | ☐ |
| Hover | <100ms | _____ ms | ☐ |
| Completion | <100ms | _____ ms | ☐ |
| Diagnostics | <500ms | _____ ms | ☐ |
| Code Actions | <100ms | _____ ms | ☐ |
| Signature Help | <50ms | _____ ms | ☐ |
| Formatting | <200ms | _____ ms | ☐ |
| Semantic Tokens | <100ms | _____ ms | ☐ |

**Overall Performance:** _____ / 13 features meet target

---

## Issues Discovered

**Issue 1:**
- **Feature:** _____________
- **Description:** _____________
- **Severity:** Critical / Major / Minor
- **Workaround:** _____________

**Issue 2:**
- **Feature:** _____________
- **Description:** _____________
- **Severity:** Critical / Major / Minor
- **Workaround:** _____________

*(Add more as needed)*

---

## Debugging Notes

### If LSP Server Doesn't Start:

1. Check Output → "NLPL Language Server" for errors
2. Check trace logs: Set `"nexuslang.trace.server": "verbose"` in settings
3. Test server manually:
   ```bash
   python3 src/nlpl/lsp/server.py
   ```
4. Verify Python path: Check `"nexuslang.languageServer.pythonPath"` setting

### If Features Don't Work:

1. Verify file is recognized as NexusLang (bottom right)
2. Check LSP capabilities in trace logs (initialize response)
3. Look for Python exceptions in Output panel
4. Try restarting extension: Ctrl+Shift+P → "Reload Window"

---

## Test Completion

**Date Tested:** __________  
**Tester:** __________  
**Duration:** _____ minutes  
**Tests Passed:** _____ / 13  
**Overall Status:** ✅ Pass / ⚠️ Partial / ❌ Fail

**Notes:**
_____________________________________________
_____________________________________________
_____________________________________________

**Next Actions:**
- [ ] Document issues in GitHub
- [ ] Update performance report with actual measurements
- [ ] Complete Week 1 review
- [ ] Plan Week 2 optimizations

---

## Quick Reference

**Keyboard Shortcuts:**
- **F12** - Go to Definition
- **Shift+F12** - Find References
- **F2** - Rename
- **Ctrl+Shift+O** - Document Outline
- **Ctrl+T** - Workspace Symbols
- **Shift+Alt+H** - Call Hierarchy
- **Ctrl+Space** - Completion
- **Ctrl+.** - Code Actions
- **Ctrl+Shift+Space** - Signature Help
- **Shift+Alt+F** - Format Document

**Output Channels:**
- **NLPL Language Server** - Server output
- **NLPL LSP Trace** - Communication trace (if verbose enabled)

**Settings Location:**
- File → Preferences → Settings → Search "NexusLang"
- Or: `.vscode/settings.json` in workspace
