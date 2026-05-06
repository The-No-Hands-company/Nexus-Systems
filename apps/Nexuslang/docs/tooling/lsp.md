# NexusLang Language Server Protocol (LSP)

## Overview

The NexusLang Language Server provides IDE-level features for NexusLang development through the Language Server Protocol (LSP). It enables powerful development tools like intelligent code completion, go-to-definition, find references, hover documentation, and workspace-wide symbol search.

## Features

### Find All References

Find all usages of a symbol (function, class, variable, method) across your entire workspace.

**Supported Symbols**:
- Functions (definitions and calls)
- Classes (definitions, instantiations, type annotations)
- Variables (assignments and references)
- Methods (definitions and calls)

**LSP Method**: `textDocument/references`

**Example**:
```nexuslang
function calculate that takes x as Integer returns Integer
 return x times 2

set result to calculate with 5 # Find references on "calculate"
set another to calculate with 10 # Will find both these calls
```

### Go-to-Definition (Enhanced)

Jump to the definition of any symbol, even across files.

**Features**:
- Cross-file navigation
- Import resolution
- Method definitions within classes
- Workspace-wide symbol lookup
- Variable definition tracking (finds closest assignment)

**LSP Method**: `textDocument/definition`

**Example**:
```nexuslang
# file: helpers.nlpl
function process_data that takes values as List returns Float
 return 0.0

# file: main.nlpl
import helpers

set result to helpers.process_data with [1, 2, 3]
# Go-to-definition jumps to helpers.nlpl
```

### Hover Documentation (Enhanced)

Rich documentation displayed on hover with parameter details, types, and examples.

**Documentation Includes**:
- Function signatures with parameter names and types
- Return types
- Standard library function documentation
- Class properties
- Variable types (explicit or inferred)
- Inline docstrings (comments after definitions)

**LSP Method**: `textDocument/hover`

**Example Hover Output**:
```markdown
**calculate** - Function

```nexuslang
function calculate that takes x as Integer, y as Float returns Float
```

**Parameters**:
- `x`: Integer
- `y`: Float

**Returns**: Float
```

**Standard Library Hover**:
```nexuslang
set result to sqrt with 16
# Hover shows: "sqrt - Square root function"
# From: math
# Returns: Float
```

### Auto-Completion (Enhanced)

Context-aware intelligent code completion.

**Completion Types**:
1. **Keywords**: NexusLang language keywords (function, class, set, if, for, etc.)
2. **Context-aware suggestions**:
 - After `set X to`: Suggests values (true, false, null, create, new)
 - After `as`: Suggests types (Integer, Float, String, List, etc.)
 - After `import` or `from`: Suggests stdlib modules
 - After `returns`: Suggests return types
3. **Variables in scope**: All variables defined in current file
4. **Functions in scope**: All functions defined in current file
5. **Classes in scope**: All classes defined in current file
6. **Standard library functions**: All stdlib module functions
7. **Snippets**: Templates for common constructs

**LSP Method**: `textDocument/completion`

**Example**:
```nexuslang
set counter to 0
set result to coun # Auto-complete suggests "counter"
# ^^^^
```

**Module Completion**:
```nexuslang
import # Shows: math, string, io, system, collections, network
```

**Type Completion**:
```nexuslang
set value as # Shows: Integer, Float, String, Boolean, List, etc.
```

### Symbol Search (Enhanced)

Fast workspace-wide symbol search with fuzzy matching and relevance scoring.

**Features**:
- Fuzzy matching (e.g., "hf" matches "helper_function")
- Symbol type filtering (functions, classes, variables, methods)
- Relevance scoring (exact matches ranked higher)
- Workspace-wide indexing

**LSP Method**: `workspace/symbol`

**Example Fuzzy Matches**:
- Query: `"ca"` Matches: `calculate`, `calculate_average`, `calibrate`
- Query: `"hf"` Matches: `helper_function`
- Query: `"proc"` Matches: `process_data`, `DataProcessor`

### Code Formatting

Automatic code formatting to maintain consistent style.

**LSP Method**: `textDocument/formatting`

### Signature Help

Display function signatures and parameter information while typing function calls.

**LSP Method**: `textDocument/signatureHelp`

### Code Actions

Quick fixes and refactoring suggestions.

**LSP Method**: `textDocument/codeAction`

### Diagnostics

Real-time syntax and semantic error detection.

**LSP Method**: `textDocument/publishDiagnostics`

## Editor Setup

### Visual Studio Code

1. **Install NLPL Language Support extension**.

2. **Manual Setup** (`.vscode/settings.json`):
```json
{
 "nexuslang.languageServer.enabled": true,
 "nexuslang.languageServer.path": "",
 "nexuslang.trace.server": "off"
}
```

3. **Launch Configuration** (`.vscode/launch.json`):
```json
{
 "type": "nlpl",
 "request": "attach",
 "name": "NLPL LSP",
 "port": 6009
}
```

### Neovim (with nvim-lspconfig)

```lua
local lspconfig = require('lspconfig')
local configs = require('lspconfig.configs')

-- Define NexusLang LSP
if not configs.nlpl then
 configs.nlpl = {
 default_config = {
 cmd = {'python', '-m', 'nexuslang.lsp', '--stdio'},
 filetypes = {'nlpl'},
 root_dir = lspconfig.util.root_pattern('.git', '.nlplroot'),
 settings = {},
 },
 }
end

-- Setup NexusLang LSP
lspconfig.nlpl.setup{}
```

### Emacs (with lsp-mode)

```elisp
(require 'lsp-mode)

(add-to-list 'lsp-language-id-configuration '(nexuslang-mode . "nlpl"))

(lsp-register-client
 (make-lsp-client
 :new-connection (lsp-stdio-connection '("python" "-m" "nexuslang.lsp" "--stdio"))
 :major-modes '(nexuslang-mode)
 :server-id 'nexuslang-lsp))

(add-hook 'nexuslang-mode-hook #'lsp)
```

### Sublime Text (with LSP package)

**Settings** (`LSP.sublime-settings`):
```json
{
 "clients": {
 "nlpl": {
 "enabled": true,
 "command": ["python", "-m", "nexuslang.lsp", "--stdio"],
 "selector": "source.nxl"
 }
 }
}
```

## Running the LSP Server

### Stdio Mode (Default)

Most editors use stdio communication:

```bash
PYTHONPATH=src python -m nexuslang.lsp --stdio
```

The server reads JSON-RPC messages from stdin and writes responses to stdout.

### Debugging

Enable debug logging:

```bash
# Logs written to /tmp/nexuslang-lsp.log
tail -f /tmp/nexuslang-lsp.log
```

## Architecture

### Components

1. **`server.py`**: Main LSP server handling JSON-RPC communication
2. **`references.py`**: Find-all-references provider (NEW)
3. **`definitions.py`**: Enhanced go-to-definition with cross-file support
4. **`hover.py`**: Enhanced hover with stdlib docs and parameter extraction
5. **`completions.py`**: Enhanced auto-completion with context awareness
6. **`symbols.py`**: Enhanced symbol search with fuzzy matching
7. **`diagnostics.py`**: Syntax and semantic error detection
8. **`formatter.py`**: Code formatting
9. **`code_actions.py`**: Quick fixes and refactoring
10. **`signature_help.py`**: Function signature hints

### LSP Protocol Flow

```
Editor LSP Server NexusLang Parser
 | | |
 |-- initialize ------------>| |
 |<- capabilities ---------- | |
 | | |
 |-- textDocument/didOpen -->| |
 | |-- parse ---------------> |
 | |<- AST/errors ---------- |
 |<- publishDiagnostics ---- | |
 | | |
 |-- textDocument/hover ---->| |
 | |-- extract symbol -------> |
 | |-- find definition -----> |
 |<- hover info ------------ | |
 | | |
 |-- textDocument/completion->| |
 |<- completion items ------ | |
```

## Standard Library Documentation

The LSP provides comprehensive documentation for all stdlib modules:

### Math Module

- `sqrt`, `sin`, `cos`, `tan`, `floor`, `ceil`, `abs`, `max`, `min`

### String Module

- `split`, `join`, `trim`, `replace`, `substring`, `length`

### I/O Module

- `read_file`, `write_file`, `print`, `input`, `read_line`

### System Module

- `exit`, `get_env`, `set_env`, `execute`

### Collections Module

- `sort`, `reverse`, `filter`, `map`, `reduce`

### Network Module

- `http_get`, `http_post`, `connect`, `listen`

## Testing

Run LSP tests:

```bash
pytest tests/test_lsp_enhancements.py -v
```

**Test Coverage**:
- 20 comprehensive tests
- All major features tested
- 100% pass rate

**Test Categories**:
1. References (3 tests): Functions, variables, classes
2. Definitions (3 tests): Functions, methods, variables
3. Hover (3 tests): Stdlib, functions with parameters, variables
4. Completions (6 tests): Context-aware, scope-based, modules
5. Symbols (5 tests): Fuzzy matching, scoring, relevance

## Example Usage

### Multi-File Project

**File: `utils.nlpl`**
```nexuslang
function helper that takes value as Integer returns Integer
 return value times 2

class DataProcessor
 property data as List of Float
 
 method process returns Float
 set sum to 0.0
 for each item in data
 set sum to sum plus item
 return sum
```

**File: `main.nlpl`**
```nexuslang
import utils

# Go-to-definition on "helper" jumps to utils.nlpl
set result to utils.helper with 5

# Find references on "DataProcessor" finds both files
set processor to new utils.DataProcessor
```

### Using LSP Features

1. **Hover** over `sqrt` See documentation: "Square root function, From: math, Returns: Float"

2. **Auto-complete** after `import` Suggests: math, string, io, system, collections, network

3. **Go-to-definition** on variable Jumps to closest assignment

4. **Find references** on function Shows all call sites

5. **Symbol search** with `"proc"` Finds: process, DataProcessor, process_data

## Troubleshooting

### LSP Server Not Starting

**Check**:
- Python path is correct
- `src/nexuslang/lsp/server.py` exists
- No syntax errors in LSP code

**Debug**:
```bash
PYTHONPATH=src python -m nexuslang.lsp --stdio
# Should start and wait for stdin input
```

### Features Not Working

**Check editor LSP client logs**:
- VSCode: Output panel "Language Server Protocol"
- Neovim: `:LspInfo`, `:LspLog`
- Emacs: `*lsp-log*` buffer

**Check server logs**:
```bash
tail -f /tmp/nexuslang-lsp.log
```

### Completions Not Showing

**Verify**:
- File is saved (some editors don't send unsaved content)
- Cursor position is correct
- LSP client is configured for NexusLang file type

## Future Enhancements

Near-term planned improvements:

- [ ] Extract function/method refactoring
- [ ] Organize imports
- [ ] Type hierarchy (`typeHierarchy/*`)
- [ ] Incremental text sync (currently full sync)

## Contributing

The LSP is implemented in pure Python and follows LSP specification 3.17.

**Adding a New Feature**:

1. Create provider in `src/nexuslang/lsp/<feature>.py`
2. Import and initialize in `server.py`
3. Add handler method `_handle_<feature>()`
4. Register in `_handle_message()` switch
5. Update server capabilities in `_handle_initialize()`
6. Add tests in `tests/tooling/test_lsp_*.py`
7. Update this documentation

## References

- [LSP Specification](https://microsoft.github.io/language-server-protocol/)
- [LSP Implementations](https://langserver.org/)
- NexusLang Documentation: `docs/`

## Status

**Version**: 0.1.0 
**Status**: Production Ready 
**Test Coverage**: 20/20 tests passing 
**Completion**: All core features implemented

**Features Summary**:
- Find All References
- Go-to-Definition (cross-file)
- Hover Documentation (enhanced)
- Auto-Completion (context-aware)
- Symbol Search (fuzzy matching)
- Diagnostics
- Code Formatting
- Signature Help
- Code Actions
