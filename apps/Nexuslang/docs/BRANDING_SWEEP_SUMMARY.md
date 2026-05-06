# Runtime Branding Sweep: NLPL → NexusLang
**Date:** 2025-02-20  
**Status:** ✅ Complete

## Overview
Updated all runtime-visible surfaces in the Python source code from "NLPL" branding to "NexusLang" branding. This ensures consistent user-facing identities for logging, diagnostics, prompts, and tool output.

## Changes by Category

### 1. Logger Names & Log Files
**Files Updated:**
- `src/nexuslang/lsp/__main__.py` - CLI logger initialization
- `src/nexuslang/lsp/server.py` - LSP server logger
- `src/nexuslang/lsp/workspace_index.py` - Workspace indexing logger

**Changes:**
- Logger names: `'nlpl-lsp'` → `'nexuslang-lsp'`
- Logger names: `'nlpl-lsp.workspace-index'` → `'nexuslang-lsp.workspace-index'`
- Log file paths: `/tmp/nlpl-lsp.log` → `/tmp/nexuslang-lsp.log`

### 2. Diagnostic Source Names
**Files Updated:**
- `src/nexuslang/lsp/server.py`
- `src/nexuslang/lsp/diagnostics.py`
- `src/nexuslang/lsp/dead_code.py`

**Changes:**
- Diagnostic sources: `"nlpl-lint"` → `"nexuslang-lint"`
- Diagnostic sources: `"nlpl-parser"` → `"nexuslang-parser"`
- Diagnostic sources: `"nlpl-typechecker"` → `"nexuslang-typechecker"`
- Diagnostic sources: `"nlpl-dead-code"` → `"nexuslang-dead-code"`

### 3. Static Analyzer Sources
**Files Updated:**
- `src/nexuslang/tooling/analyzer/ide_hooks.py`

**Changes:**
- Analyzer source: `"nlpl-analyze"` → `"nexuslang-analyze"`
- Default parameter: `"nlpl-analyze"` → `"nexuslang-analyze"` (2 locations)

### 4. Class Names
**Files Updated:**
- `src/nexuslang/lsp/server.py` (8 edits)
- `src/nexuslang/lsp/__init__.py` (2 edits)
- `src/nexuslang/lsp/__main__.py` (2 edits)
- `src/nexuslang/lsp/formatter.py` (3 edits)
- `src/nexuslang/lsp/code_actions.py` (2 edits)
- `src/nexuslang/errors.py` (9 edits)
- `src/nexuslang/interpreter/interpreter.py` (8 edits)

**Changes:**
- Class: `NLPLLanguageServer` → `NexusLangLanguageServer` (8 usages)
- Class: `NLPLFormatter` → `NexusLangFormatter` (6 usages)
- Exception: `NLPLContractError` → `NxlContractError` (9 usages)

### 5. CLI Descriptions & Help Text
**Files Updated:**
- `src/nexuslang/lsp/__main__.py` - LSP server CLI
- `src/nexuslang/debugger/debugger.py` - Debugger CLI
- `src/nexuslang/cli/nlpllint.py` - Linter CLI

**Changes:**
- Argparse description: `"NLPL Language Server..."` → `"NexusLang Language Server..."`
- Argparse description: `"NLPL Debugger"` → `"NexusLang Debugger"`
- Argparse description: `"NLPL Static Analyzer..."` → `"NexusLang Static Analyzer..."`

### 6. User-Facing Prompts
**Files Updated:**
- `src/nexuslang/debugger/debugger.py`
- `src/nexuslang/lsp/completions.py`

**Changes:**
- Prompt: `"(nlpl-dbg)"` → `"(nexuslang-dbg)"`
- Completion detail: `"NLPL keyword"` → `"NexusLang keyword"`

### 7. Module Docstrings
**Files Updated:**
- `src/nexuslang/lsp/__init__.py`
- `src/nexuslang/lsp/__main__.py`
- `src/nexuslang/lsp/formatter.py`
- `src/nexuslang/lsp/server.py`
- `src/nexuslang/tools/__init__.py`
- `src/nexuslang/tools/analyzer.py`
- `src/nexuslang/tools/profiler.py`
- `src/nexuslang/tooling/analyzer/__init__.py`
- `src/nexuslang/jit/__init__.py`
- `src/nexuslang/debugger/__init__.py`
- `src/nexuslang/debugger/__main__.py`
- `src/nexuslang/debugger/debugger.py`
- `src/nexuslang/compiler/__init__.py`
- `src/nexuslang/cli/__init__.py`
- `src/nexuslang/lsp/completions.py` - Completion details

**Changes:**
- Docstring headers updated throughout:
  - `"NLPL Language Server..."` → `"NexusLang Language Server..."`
  - `"NLPL Debugger..."` → `"NexusLang Debugger..."`
  - `"NLPL Compiler..."` → `"NexusLang Compiler..."`
  - `"NLPL Code Formatter"` → `"NexusLang Code Formatter"`
  - `"NLPL Static Analyzer"` → `"NexusLang Static Analyzer"`
  - `"NLPL JIT Compiler"` → `"NexusLang JIT Compiler"`
  - `"NLPL Profiler"` → `"NexusLang Profiler"`
  - `"NLPL Tools Package"` → `"NexusLang Tools Package"`

### 8. Error/Debug Output
**Files Updated:**
- `src/nexuslang/main.py` - Error code header
- `src/nexuslang/debugger/debug_info.py` - Producer metadata

**Changes:**
- Output: `"NLPL Error Codes"` → `"NexusLang Error Codes"`
- Producer metadata: `"NLPL Compiler"` → `"NexusLang Compiler"`

## Verification
✅ All Python files compile without syntax errors  
✅ All imports properly updated  
✅ All class names consistently renamed  
✅ Logger names standardized  
✅ Diagnostic sources use consistent naming  
✅ User-facing strings updated  

## Not Changed (Intentional)
- **Module paths**: `nexuslang` package structure remains stable
- **File extensions**: `.nlpl` remains as the source file extension
- **Comments & internal documentation**: Left unchanged to avoid noise
- **Test fixtures & internal classes**: Left unchanged where not user-visible
- **Comments in docstrings**: Mostly preserved for reference

## Runtime Impact
All user-visible output (logs, error messages, IDE diagnostics, CLI prompts) now consistently displays "NexusLang" branding instead of "NLPL", providing users with a cohesive experience across all tooling surfaces.

## Backward Compatibility
⚠️ **Breaking Changes:**
- Internal class names changed: `NLPLLanguageServer` → `NexusLangLanguageServer`
- Internal class names changed: `NLPLFormatter` → `NexusLangFormatter`
- Internal exception changed: `NLPLContractError` → `NxlContractError`

Any external code importing these classes directly will need to be updated. However, since these are internal implementation classes, users typically interact through published APIs and shouldn't be affected.
